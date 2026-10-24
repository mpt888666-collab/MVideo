//
// Chat login business logic.
//

#include "service/LoginService.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "common/ThreadPool.h"
#include "const.h"
#include "logic/LogicSystem.h"
#include "net/CSession.h"
#include "redis/ChatRedis.h"
#include "redis/RedisCleanup.h"
#include "redis/RedisMgr.h"

namespace {

using json = nlohmann::json;

class LoginAttemptGuard {
public:
    LoginAttemptGuard(std::shared_ptr<CSession> session, std::uint64_t attempt)
        : _session(std::move(session)),
          _attempt(attempt) {}

    ~LoginAttemptGuard() {
        if (_active && _session) {
            _session->EndLogin(_attempt);
        }
    }

    void Dismiss() { _active = false; }

private:
    std::shared_ptr<CSession> _session;
    std::uint64_t _attempt;
    bool _active{true};
};

bool IsValidToken(const std::string& token) {
    if (token.size() != 32) {
        return false;
    }
    return std::all_of(token.begin(), token.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

int MapConsumeError(long long code) {
    switch (code) {
        case -2:
        case -3:
        case -4: return ErrorCodes::ChatTokenInvalid;
        case -5: return ErrorCodes::ChatTokenUsed;
        case -6:
        case -7: return ErrorCodes::ChatTokenExpired;
        default: return ErrorCodes::ChatRedisError;
    }
}

void SendLoginReply(
        const std::shared_ptr<CSession>& session, int error, std::uint64_t user_id = 0, std::int64_t active = 0
) {
    if (!session) {
        return;
    }

    json response;
    response["ec"] = error;
    if (error == ErrorCodes::Success) {
        response["user_id"] = user_id;
        response["session_id"] = session->GetSessionId();
        response["active"] = active;
    }
    session->Send(response.dump(), MSG_ID::ID_CHAT_LOGIN_QSP);
}

void CompleteLogin(
        const std::shared_ptr<CSession>& session, const std::string& token, std::uint64_t user_id,
        std::uint64_t attempt, const std::vector<long long>& result
) {
    if (!session) {
        return;
    }
    session->EndLogin(attempt);

    if (result.empty()) {
        SendLoginReply(session, ErrorCodes::ChatRedisError);
        return;
    }
    if (result[0] != 1) {
        SendLoginReply(session, MapConsumeError(result[0]));
        return;
    }

    if (!session->IsOpen()) {
        redis_cleanup::QueueRelease(token);
        return;
    }

    if (!session->SetLoginState(user_id, token)) {
        redis_cleanup::QueueRelease(token);
        return;
    }

    const std::int64_t active = result.size() > 1 ? result[1] : 0;
    SendLoginReply(session, ErrorCodes::Success, user_id, active);
}

}  // namespace

void service::LoginService::Handle(const std::shared_ptr<CSession>& session, const std::string& msg_data) {
    if (!session || !session->IsOpen()) {
        return;
    }

    const std::uint64_t attempt = session->TryBeginLogin();
    if (attempt == 0) {
        SendLoginReply(session, ErrorCodes::ChatAlreadyLoggedIn);
        return;
    }

    LoginAttemptGuard attempt_guard(session, attempt);

    json request;
    try {
        request = json::parse(msg_data);
    } catch (const json::exception&) {
        SendLoginReply(session, ErrorCodes::Error_Json);
        return;
    }
    if (!request.is_object()) {
        SendLoginReply(session, ErrorCodes::Error_Json);
        return;
    }

    std::string token;
    unsigned long long user_id = 0;
    try {
        token = request.value("token", std::string{});
        user_id = request.value("user_id", 0ULL);
    } catch (const json::exception&) {
        SendLoginReply(session, ErrorCodes::Error_Json);
        return;
    }

    if (!IsValidToken(token) || user_id == 0) {
        SendLoginReply(session, ErrorCodes::ChatTokenInvalid);
        return;
    }

    const auto enqueue_at = std::chrono::steady_clock::now();
    const bool submitted = redisPool::TrySubmit([session, token, user_id, attempt, enqueue_at]() {
        LoginAttemptGuard redis_guard(session, attempt);

        const auto now = std::chrono::steady_clock::now();
        if (now - enqueue_at > std::chrono::milliseconds(redisPool::kRedisQueueBudgetMs)) {
            SendLoginReply(session, ErrorCodes::TooManyRequests);
            return;
        }

        auto redis = RedisMgr::GetInstance();
        if (!redis->IsValid()) {
            SendLoginReply(session, ErrorCodes::ChatRedisError);
            return;
        }

        auto result = redis->EvalInts(
                chat_redis::kConsumeLoginScript,
                {chat_redis::PendingKey(), chat_redis::ActiveKey(), chat_redis::TokenKey(token)},
                {token, std::to_string(user_id), chat_redis::ServerName(), session->GetSessionId()}
        );

        if (result.empty()) {
            SendLoginReply(session, ErrorCodes::ChatRedisError);
            return;
        }

        const bool consumed = result[0] == 1;
        bool handed_off = false;
        try {
            handed_off = LogicSystem::GetInstance()->PostContinuation(
                    session->GetSessionId(),
                    [session, token, user_id, attempt, result = std::move(result)]() {
                        CompleteLogin(session, token, user_id, attempt, result);
                    }
            );
        } catch (...) {
            handed_off = false;
        }

        if (!handed_off) {
            if (consumed) {
                redis_cleanup::QueueRelease(token);
            }
            SendLoginReply(session, ErrorCodes::TooManyRequests);
            return;
        }

        redis_guard.Dismiss();
    });

    if (!submitted) {
        SendLoginReply(session, ErrorCodes::TooManyRequests);
        return;
    }

    attempt_guard.Dismiss();
}
