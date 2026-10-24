#include "service/RegisterService.h"

#include "redis/RedisMgr.h"
#include "common/Validators.h"
#include "const.h"
#include "db/MySqlMgr.h"
#include "redis/RateLimiter.h"
#include "redis/VarifyCodeStore.h"
#include "security/PasswordUtil.h"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace service {
namespace {
RegisterResult ErrorResult(int error, std::string message, int retry_after = 0) {
    return RegisterResult{error, 0, retry_after, std::move(message)};
}

int RegisterRetryAfter(int hit) {
    switch (hit) {
        case 1: return 1;      // 全局每秒
        case 2: return 60;     // IP 每分钟
        case 3: return 3600;   // IP 每小时
        case 4: return 3600;   // 邮箱每小时
        case 5: return 86400;  // 邮箱每天
        default: return 60;
    }
}
}  // namespace

RegisterResult RegisterService::Admit(
        const RegisterRequest& request, const std::string& client_ip, RegisterRequest& normalized
) {
    normalized = RegisterRequest{validators::TrimAscii(request.account),  request.password,
                                 validators::TrimAscii(request.username), validators::NormalizeEmail(request.email),
                                 validators::TrimAscii(request.phone),    validators::TrimAscii(request.code)};

    if (!validators::IsValidAccount(normalized.account)) {
        return ErrorResult(ErrorCodes::Error_Account, "账号必须是 4-32 位字母、数字或下划线");
    }
    if (!validators::IsValidPassword(normalized.password)) {
        return ErrorResult(ErrorCodes::Error_Password, "密码长度必须是 8-128 字节");
    }
    if (!validators::IsValidUsername(normalized.username)) {
        return ErrorResult(ErrorCodes::Error_Username, "昵称不能为空且不能超过 64 个字符");
    }
    if (normalized.email.size() > 128 || !validators::IsValidEmail(normalized.email)) {
        return ErrorResult(ErrorCodes::Error_Email, "邮箱格式非法");
    }
    if (!validators::IsValidPhone(normalized.phone)) {
        return ErrorResult(ErrorCodes::Error_Phone, "手机号格式非法");
    }
    if (!validators::IsValidCode(normalized.code)) {
        return ErrorResult(ErrorCodes::Error_Code, "验证码格式非法");
    }
    if (client_ip.empty()) {
        return ErrorResult(ErrorCodes::IPUnknown, "无法获取客户端 IP");
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return ErrorResult(ErrorCodes::VarifyBackErr, "验证码服务暂不可用");
    }

    const auto now_tp = std::chrono::system_clock::now().time_since_epoch();
    const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(now_tp).count();
    const std::vector<redis::RateRule> rules{
            {RegGlobalCdKey + std::to_string(now_sec), kRegGlobalPerSec, std::chrono::seconds(2)},
            {RegIpCdKeyMinute + client_ip, kRegIpPerMinute, std::chrono::seconds(60)},
            {RegIpCdKeyHour + client_ip, kRegIpPerHour, std::chrono::seconds(3600)},
            {RegMailCdKeyHour + normalized.email, kRegMailPerHour, std::chrono::seconds(3600)},
            {RegMailCdKeyDay + normalized.email, kRegMailPerDay, std::chrono::seconds(86400)},
    };
    const int hit = redis::RateLimiter::Acquire(*redis, rules);
    if (hit < 0) {
        return ErrorResult(ErrorCodes::VarifyBackErr, "限流服务暂不可用");
    }
    if (hit > 0) {
        return ErrorResult(ErrorCodes::TooManyRequests, "注册请求过于频繁", RegisterRetryAfter(hit));
    }

    return RegisterResult{ErrorCodes::Success, 0, 0, {}};
}

RegisterResult RegisterService::Register(const RegisterRequest& normalized) {
    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return ErrorResult(ErrorCodes::VarifyBackErr, "验证码服务暂不可用");
    }

    const auto code_status = redis::VarifyCodeStore::Check(*redis, normalized.email, normalized.code);
    switch (code_status) {
        case redis::VarifyCodeStatus::Match: break;
        case redis::VarifyCodeStatus::Mismatch: return ErrorResult(ErrorCodes::Error_Code, "验证码错误");
        case redis::VarifyCodeStatus::NotFound: return ErrorResult(ErrorCodes::Error_Code, "验证码已过期，请重新获取");
        default: return ErrorResult(ErrorCodes::VarifyBackErr, "验证码服务暂不可用");
    }

    std::string password_hash;
    if (!security::PasswordUtil::Hash(normalized.password, password_hash)) {
        return ErrorResult(ErrorCodes::DBError, "注册失败，请稍后重试");
    }

    MySqlMgr::NewUser new_user;
    new_user.account = normalized.account;
    new_user.username = normalized.username;
    new_user.email = normalized.email;
    new_user.phone = normalized.phone;
    new_user.password_hash = password_hash;

    long long user_id = 0;
    const auto create_result = MySqlMgr::GetInstance()->CreateUser(new_user, user_id);
    switch (create_result) {
        case MySqlMgr::CreateUserResult::Success: break;
        case MySqlMgr::CreateUserResult::Duplicate:
            return ErrorResult(ErrorCodes::UserExists, "账号、邮箱或手机号已被注册");
        default: return ErrorResult(ErrorCodes::DBError, "注册失败，请稍后重试");
    }

    redis::VarifyCodeStore::Consume(*redis, normalized.email, normalized.code);
    return RegisterResult{ErrorCodes::Success, user_id, 0, "注册成功"};
}
}  // namespace service
