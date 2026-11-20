//
// Created by mpt on 2026/11/20.
//

#include "PlayVideoService.h"

#include "../const.h"
#include "../redis/RateLimiter.h"
#include "../redis/RedisMgr.h"

namespace {

bool IsValidToken(const std::string& token) {
    if (token.size() != 32) {
        return false;
    }
    for (const unsigned char ch : token) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
            return false;
        }
    }
    return true;
}

}

namespace service {

int PlayVideoService::Admit(const PlayVideoRequest& request, const std::string& ip) {
    if (ip.empty()) {
        return ErrorCodes::IPUnknown;
    }
    if (request.userId == 0 || request.videoId == 0 || !IsValidToken(request.token)) {
        return ErrorCodes::Error_Token;
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return ErrorCodes::VarifyBackErr;
    }

    const int matched = redis->MatchHash(
            std::string(ChatTokenKey) + request.token,
            {{"user_id", std::to_string(request.userId)}, {"state", "active"}});
    if (matched < 0) {
        return ErrorCodes::VarifyBackErr;
    }
    if (matched == 0) {
        return ErrorCodes::Error_Token;
    }

    const std::vector<redis::RateRule> rules{
            {PlayVideoUserPerMinute + std::to_string(request.userId),
                    kPlayVideoUserPerMinute, std::chrono::seconds(60)},
            {PlayVideoIpPerMinute + ip,
                    kPlayVideoIpPerMinute, std::chrono::seconds(60)}
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

const char* PlayVideoService::ErrorMessage(int error) {
    switch (error) {
        case ErrorCodes::Success:
            return "";
        case ErrorCodes::Error_Token:
            return "登录状态已失效，请重新登录";
        case ErrorCodes::TooManyRequests:
            return "请求过于频繁，请稍后再试";
        case ErrorCodes::VideoNotFound:
            return "视频不存在、未发布或仍在处理中";
        case ErrorCodes::StorageError:
            return "存储服务暂时不可用";
        case ErrorCodes::DBError:
        case ErrorCodes::MySqlError:
            return "数据库操作失败";
        case ErrorCodes::VarifyBackErr:
            return "服务暂时不可用";
        case ErrorCodes::Error_Json:
            return "请求参数不合法";
        default:
            return "获取播放地址失败";
    }
}

} // namespace service