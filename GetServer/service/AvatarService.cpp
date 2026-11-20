#include "AvatarService.h"

#include <cctype>
#include <chrono>
#include <string>

#include "const.h"
#include "db/MySqlMgr.h"
#include "redis/RateLimiter.h"
#include "redis/RedisMgr.h"
#include "storage/MinioClient.h"

namespace service {
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

bool IsValidRequestId(const std::string& value) {
    if (value.size() < 8 || value.size() > 64) {
        return false;
    }
    for (const unsigned char ch : value) {
        if (!(std::isalnum(ch) || ch == '-' || ch == '_')) {
            return false;
        }
    }
    return true;
}

int ValidateUser(unsigned long long uid, const std::string& token) {
    if (uid == 0 || !IsValidToken(token)) {
        return ErrorCodes::Error_Token;
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis || !redis->IsValid()) {
        return ErrorCodes::VarifyBackErr;
    }

    const int matched = redis->MatchHash(
            std::string(ChatTokenKey) + token,
            {{"user_id", std::to_string(uid)}, {"state", "active"}});
    if (matched < 0) {
        return ErrorCodes::VarifyBackErr;
    }
    if (matched == 0) {
        return ErrorCodes::Error_Token;
    }
    return ErrorCodes::Success;
}

int AcquireLimit(unsigned long long uid, const std::string& ip) {
    auto redis = RedisMgr::GetInstance();
    if (!redis || !redis->IsValid()) {
        return ErrorCodes::VarifyBackErr;
    }

    const std::vector<redis::RateRule> rules{
            {std::string(UploadAvatarUserPerMinute) + std::to_string(uid),
             kUploadAvatarUserPerMinute, std::chrono::seconds(60)},
            {std::string(UploadAvatarIpPerMinute) + ip,
             kUploadAvatarIpPerMinute, std::chrono::seconds(60)}
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

}  // namespace

AvatarTicketResponse AvatarService::CreateTicket(
        const AvatarTicketRequest& request, const std::string& clientIp
) {
    AvatarTicketResponse response;
    if (clientIp.empty()) {
        response.ec = ErrorCodes::IPUnknown;
        response.message = "client ip unknown";
        return response;
    }
    if (!IsValidRequestId(request.requestId)) {
        response.ec = ErrorCodes::Error_Json;
        response.message = "invalid request_id";
        return response;
    }
    if (request.fileSize == 0 || request.fileSize > kUploadAvatarMaxBytes) {
        response.ec = ErrorCodes::Error_Json;
        response.message = "avatar must be 1 byte - 2 MB";
        return response;
    }

    response.ec = ValidateUser(request.uid, request.token);
    if (response.ec != ErrorCodes::Success) {
        response.message = "invalid login token";
        return response;
    }
    response.ec = AcquireLimit(request.uid, clientIp);
    if (response.ec != ErrorCodes::Success) {
        response.message = "too many avatar uploads";
        return response;
    }

    auto& minio = MinioClient::Instance();
    if (!minio.IsConfigured()) {
        response.ec = ErrorCodes::StorageError;
        response.message = "object storage unavailable";
        return response;
    }

    response.objectKey = "avatars/" + std::to_string(request.uid) + "/" +
                         request.requestId + ".jpg";
    response.uploadUrl = minio.PresignPut(response.objectKey, kUploadAvatarUrlTtlSec);
    if (response.uploadUrl.empty()) {
        response.ec = ErrorCodes::StorageError;
        response.message = "cannot sign upload url";
        response.objectKey.clear();
        return response;
    }

    response.expiresIn = kUploadAvatarUrlTtlSec;
    response.ec = ErrorCodes::Success;
    return response;
}

AvatarCompleteResponse AvatarService::Complete(
        unsigned long long uid, const std::string& token,
        const std::string& objectKey, const std::string& clientIp
) {
    AvatarCompleteResponse response;
    if (clientIp.empty()) {
        response.ec = ErrorCodes::IPUnknown;
        response.message = "client ip unknown";
        return response;
    }

    response.ec = ValidateUser(uid, token);
    if (response.ec != ErrorCodes::Success) {
        response.message = "invalid login token";
        return response;
    }

    const std::string prefix = "avatars/" + std::to_string(uid) + "/";
    if (objectKey.size() > 512 || objectKey.rfind(prefix, 0) != 0 ||
        objectKey.find("..") != std::string::npos ||
        objectKey.size() <= prefix.size() + 4 ||
        objectKey.compare(objectKey.size() - 4, 4, ".jpg") != 0) {
        response.ec = ErrorCodes::Error_Json;
        response.message = "invalid avatar object key";
        return response;
    }

    auto& minio = MinioClient::Instance();
    const auto size = minio.HeadObjectSize(objectKey, 5);
    if (!size || *size == 0 || *size > kUploadAvatarMaxBytes) {
        response.ec = ErrorCodes::UploadIncomplete;
        response.message = "avatar object is missing or too large";
        return response;
    }

    if (!MySqlMgr::GetInstance()->UpdateUserAvatar(uid, objectKey)) {
        response.ec = ErrorCodes::DBError;
        response.message = "failed to save avatar";
        return response;
    }

    response.avatarKey = objectKey;
    response.avatarUrl = minio.PresignGet(objectKey, kUploadAvatarUrlTtlSec);
    response.ec = ErrorCodes::Success;
    return response;
}

}  // namespace service