//
// Created by mpt on 2026/10/18.
//

#include "ResetPwdService.h"

#include "const.h"
#include "common/Validators.h"
#include "db/MySqlMgr.h"
#include "redis/RateLimiter.h"
#include "redis/RedisMgr.h"
#include "redis/VarifyCodeStore.h"
#include "security/PasswordUtil.h"

#include <chrono>
#include <string>
#include <vector>

namespace service {
namespace {
int ResetPwdRetryAfter(int hit) {
    switch (hit) {
        case 1: return 1;
        case 2: return 60;
        case 3: return 3600;
        case 4: return 3600;
        case 5: return 86400;
        default: return 60;
    }
}
}  // namespace

ResetPwdResponse ResetPwdService::Admit(
        const ResetPwdRequest& request, const std::string& client_ip, ResetPwdRequest& normalized
) {
    normalized = ResetPwdRequest{
            validators::NormalizeEmail(request.email), validators::TrimAscii(request.code), request.new_password,
            request.confirm
    };

    if (!validators::IsValidEmail(normalized.email)) {
        return ResetPwdResponse(ErrorCodes::Error_Email, 0);
    }
    if (!validators::IsValidCode(normalized.code)) {
        return ResetPwdResponse(ErrorCodes::Error_Code, 0);
    }
    if (!validators::IsSamePwd(normalized.new_password, normalized.confirm)) {
        return ResetPwdResponse(ErrorCodes::ErrorPwdNotSame, 0);
    }
    if (!validators::IsValidPassword(normalized.new_password) || !validators::IsValidPassword(normalized.confirm)) {
        return ResetPwdResponse(ErrorCodes::Error_Password, 0);
    }
    if (client_ip.empty()) {
        return ResetPwdResponse(ErrorCodes::IPUnknown, 0);
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return ResetPwdResponse(ErrorCodes::VarifyBackErr, 0);
    }

    const auto now_tp = std::chrono::system_clock::now().time_since_epoch();
    const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now_tp).count();
    const std::vector<redis::RateRule> rules{
            {ResetGlobalCdKey + std::to_string(now_sec), kResetGlobalPerSec, std::chrono::seconds(2)},
            {ResetIpCdKeyMinute + client_ip, kResetIpPerMinute, std::chrono::seconds(60)},
            {ResetIpCdKeyHour + client_ip, kResetIpPerHour, std::chrono::seconds(3600)},
            {ResetMailCdKeyHour + normalized.email, kResetMailPerHour, std::chrono::seconds(3600)},
            {ResetMailCdKeyDay + normalized.email, kResetMailPerDay, std::chrono::seconds(86400)}
    };

    const int hit = redis::RateLimiter::Acquire(*redis, rules);
    if (hit < 0) {
        return ResetPwdResponse(ErrorCodes::VarifyBackErr, 0);
    }
    if (hit > 0) {
        return ResetPwdResponse(ErrorCodes::TooManyRequests, ResetPwdRetryAfter(hit));
    }
    return ResetPwdResponse(ErrorCodes::Success, 0);
}

ResetPwdResponse ResetPwdService::ResetPwd(const ResetPwdRequest& normalized) {
    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return ResetPwdResponse(ErrorCodes::VarifyBackErr, 0);
    }

    switch (redis::VarifyCodeStore::Check(*redis, normalized.email, normalized.code)) {
        case redis::VarifyCodeStatus::Match: break;
        case redis::VarifyCodeStatus::Mismatch:
        case redis::VarifyCodeStatus::NotFound: return ResetPwdResponse(ErrorCodes::Error_Code, 0);
        default: return ResetPwdResponse(ErrorCodes::VarifyBackErr, 0);
    }

    std::string password_hash;
    if (!security::PasswordUtil::Hash(normalized.new_password, password_hash)) {
        return ResetPwdResponse(ErrorCodes::DBError, 0);
    }

    const int db_result = MySqlMgr::GetInstance()->ResetPwd(normalized.email, password_hash);
    if (db_result < 0) {
        return ResetPwdResponse(ErrorCodes::MySqlError, 0);
    }
    if (db_result > 0) {
        return ResetPwdResponse(ErrorCodes::Error_Code, 0);
    }

    redis::VarifyCodeStore::Consume(*redis, normalized.email, normalized.code);
    return ResetPwdResponse(ErrorCodes::Success, 0);
}
}  // namespace service
