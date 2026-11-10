//
// Created by mpt on 2026/11/10.
//

#include "CheckStatusServer.h"

#include "../const.h"
#include "../redis/RateLimiter.h"
#include "../redis/RedisMgr.h"

namespace {
    bool IsValidToken(const std::string& token) {
        if (token.size() != 32) {
            return false;
        }
        for (const unsigned char ch: token) {
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
                return false;
            }
        }
        return true;
    }
}

namespace service {
    int CheckStatusServer::Admit(const RequestData& data) {
        if (data.ip.empty()) {
            return ErrorCodes::IPUnknown;
        }
        if (data.userId == 0 || !IsValidToken(data.token)) {
            return ErrorCodes::Error_Token;
        }

        auto redis = RedisMgr::GetInstance();
        if (!redis->IsValid()) {
            return ErrorCodes::VarifyBackErr;
        }

        const int matched = redis->MatchHash(
                std::string(ChatTokenKey) + data.token,
                {{"user_id", std::to_string(data.userId)}, {"state", "active"}});
        if (matched < 0) {
            return ErrorCodes::VarifyBackErr;
        }
        if (matched == 0) {
            return ErrorCodes::Error_Token;
        }
        const std::vector<redis::RateRule> rules{
                {CheckVideoStatusUserPerSeconde + std::to_string(data.userId),
                        kCheckVideoStatusUserPerSeconde, std::chrono::seconds(60)},
                {CheckVideoStatusIpPerMinute + data.ip,
                        kCheckVideoStatusIpPerMinute, std::chrono::seconds(60)}
        };
        const int hit = redis::RateLimiter::Acquire(*redis, rules);
        if (hit < 0) {
            return ErrorCodes::VarifyBackErr;
        }
        if (hit > 0) {
            return ErrorCodes::TooManyRequests;
        }

        return ErrorCodes::Success;
    }
}