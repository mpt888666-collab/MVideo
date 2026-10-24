//
// Created by mpt on 2026/10/18.
//

#include "LogicService.h"

#include "common/Validators.h"
#include "const.h"
#include "db/MySqlMgr.h"
#include "http/PostToPool.h"
#include "redis/RateLimiter.h"
#include "redis/RedisMgr.h"
#include "rpc/StatusGrpcClient.h"
#include "rpc/VarifyGrpcClient.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace service {
namespace {
int LoginRetryAfter(int hit) {
    switch (hit) {
        case 1: return 1;
        case 2: return 60;
        case 3: return 3600;
        case 4: return 3600;
        case 5: return 86400;
        default: return 60;
    }
}

std::string MakeRequestId(unsigned long long user_id) {
    const auto now_tp = std::chrono::steady_clock::now().time_since_epoch();
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(now_tp).count();
    return std::to_string(user_id) + "-" + std::to_string(now);
}
}  // namespace

LogicResponse LogicService::Admit(const LogicRequest& request, const std::string& client_ip, LogicRequest& normalized) {
    normalized = LogicRequest{validators::NormalizeEmail(request.email), request.password, request.request_id};

    if (client_ip.empty()) {
        return LogicResponse(ErrorCodes::IPUnknown, 0, 0);
    }
    if (!validators::IsValidEmail(normalized.email)) {
        return LogicResponse(ErrorCodes::Error_Email, 0, 0);
    }
    if (normalized.password.empty() || normalized.password.size() > 128) {
        return LogicResponse(ErrorCodes::ErrorPwdNotSuccess, 0, 0);
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return LogicResponse(ErrorCodes::VarifyBackErr, 0, 0);
    }

    const auto now_tp = std::chrono::system_clock::now().time_since_epoch();
    const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now_tp).count();
    const std::vector<redis::RateRule> rules{
            {LogicGlobalCdKey + std::to_string(now_sec), kLogicGlobalPerSec, std::chrono::seconds(2)},
            {LogicIpCdKeyMinute + client_ip, kLogicIpPerMinute, std::chrono::seconds(60)},
            {LogicIpCdKeyHour + client_ip, kLogicIpPerHour, std::chrono::seconds(3600)},
            {LogicMailCdKeyHour + normalized.email, kLogicMailPerHour, std::chrono::seconds(3600)},
            {LogicMailCdKeyDay + normalized.email, kLogicMailPerDay, std::chrono::seconds(86400)}
    };

    const int hit = redis::RateLimiter::Acquire(*redis, rules);
    if (hit < 0) {
        return LogicResponse(ErrorCodes::VarifyBackErr, 0, 0);
    }
    if (hit > 0) {
        return LogicResponse(ErrorCodes::TooManyRequests, LoginRetryAfter(hit), 0);
    }
    return LogicResponse(ErrorCodes::Success, 0, 0);
}

void LogicService::Logic(const LogicRequest& normalized, std::shared_ptr<HttpConnection> connection, LogicDone done) {
    auto replied = std::make_shared<std::atomic<bool>>(false);
    auto done_once = [replied, done = std::move(done)](const LogicResponse& response) {
        bool expected = false;
        if (!replied->compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }
        done(response);
    };

    unsigned long long user_id = 0;
    const int login_result = MySqlMgr::GetInstance()->UserLogin(normalized.email, normalized.password, user_id);
    if (login_result < 0) {
        done_once(LogicResponse(ErrorCodes::DBError));
        return;
    }
    if (login_result != 0) {
        done_once(LogicResponse(ErrorCodes::ErrorPwdNotSuccess));
        return;
    }

    auto redis = RedisMgr::GetInstance();
    if (redis->IsValid()) {
        redis->Del(LogicMailCdKeyHour + normalized.email);
        redis->Del(LogicMailCdKeyDay + normalized.email);
    }

    const std::string request_id = normalized.request_id.empty() ? MakeRequestId(user_id) : normalized.request_id;

    http::PostToPool(
            rpc::RpcWorkerPool(), rpc::g_rpcQueued, rpc::RpcQueueLimit(), rpc::kRpcQueueBudgetMs, std::move(connection),
            [done_once]() { done_once(LogicResponse(ErrorCodes::TooManyRequests, 1, 0)); },
            [done_once]() { done_once(LogicResponse(ErrorCodes::RPCFailed, 0, 0)); }, {},
            [user_id, request_id, done_once]() {
                const auto route = StatusGrpcClient::GetInstance()->GetChatServer(user_id, request_id);
                if (route.error() != ErrorCodes::Success) {
                    done_once(LogicResponse(ErrorCodes::RPCFailed, 0, user_id));
                    return;
                }

                LogicResponse response;
                response.ec = ErrorCodes::Success;
                response.user_id = user_id;
                response.host = route.host();
                response.port = route.port();
                response.token = route.token();
                done_once(response);
            }
    );
}
}  // namespace service
