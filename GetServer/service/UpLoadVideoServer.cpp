//
// Created by mpt on 2026/10/27.
//

#include "UpLoadVideoServer.h"

#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>

#include "db/MySqlMgr.h"
#include "redis/RateLimiter.h"
#include "redis/RedisMgr.h"
#include "storage/MinioClient.h"

namespace service {
namespace {

using json = nlohmann::json;

constexpr int kUploadUrlTtlSec = 900;

// 上传会话快照：后面 /video_upload_complete 要靠它确认 object_key 的归属
std::string BuildTicketPayload(const RequestVideoData& request, const std::string& client_ip) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

    json payload;
    payload["uid"] = request.uid;
    payload["title"] = request.title;
    payload["file_name"] = request.fileName;
    payload["file_size"] = request.fileSize;
    payload["tags"] = request.tags;
    payload["ip"] = client_ip;
    payload["created_at"] = now;
    return payload.dump();
}

std::string NormalizeExtension(const std::string& file_name) {
    const auto dot = file_name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= file_name.size()) {
        return "mp4";
    }

    std::string extension = file_name.substr(dot + 1);
    if (extension.size() > 10) {
        return "mp4";
    }
    for (auto& ch : extension) {
        const auto value = static_cast<unsigned char>(ch);
        if (!std::isalnum(value)) {
            return "mp4";
        }
        ch = static_cast<char>(std::tolower(value));
    }
    return extension;
}

}  // namespace

boost::beast::http::status HttpStatusForError(int error) {
    switch (error) {
        case ErrorCodes::Success: return boost::beast::http::status::ok;
        case ErrorCodes::TooManyRequests: return boost::beast::http::status::too_many_requests;
        case ErrorCodes::UploadDuplicated: return boost::beast::http::status::conflict;
        case ErrorCodes::VideoNotFound: return boost::beast::http::status::not_found;
        case ErrorCodes::UploadObjectMismatch:
        case ErrorCodes::UploadIncomplete: return boost::beast::http::status::conflict;
        case ErrorCodes::StorageError: return boost::beast::http::status::service_unavailable;
        case ErrorCodes::UserExists: return boost::beast::http::status::conflict;
        case ErrorCodes::RPCFailed: return boost::beast::http::status::bad_gateway;
        case ErrorCodes::VarifyBackErr: return boost::beast::http::status::service_unavailable;
        case ErrorCodes::DBError:
        case ErrorCodes::MySqlError: return boost::beast::http::status::internal_server_error;
        default: return boost::beast::http::status::bad_request;
    }
}

std::string MakeUploadObjectKey(const RequestVideoData& request) {
    if (request.uid == 0 || request.requestId.empty()) {
        return {};
    }
    return "videos/" + std::to_string(request.uid) + "/" + request.requestId + "." +
           NormalizeExtension(request.fileName);
}

std::optional<unsigned long long> InsertUploadingVideo(const RequestVideoData& request, const std::string& object_key) {
    if (request.uid == 0 || request.requestId.empty() || request.title.empty() ||
        request.fileSize <= 0 || object_key.empty()) {
        return std::nullopt;
    }

    MySqlMgr::NewVideo video{};
    video.author_id = request.uid;
    video.request_id = request.requestId;
    video.title = request.title;
    video.description = request.description;
    video.object_key = object_key;
    video.file_size = static_cast<unsigned long long>(request.fileSize);
    video.tags = request.tags;

    return MySqlMgr::GetInstance()->CreateUploadingVideo(video);
}

std::optional<UploadSession> CreateUploadSession(const RequestVideoData& request) {
    const std::string object_key = MakeUploadObjectKey(request);
    if (object_key.empty()) {
        return std::nullopt;
    }

    auto& minio = MinioClient::Instance();
    if (!minio.IsConfigured()) {
        return std::nullopt;
    }

    const std::string upload_url = minio.PresignPut(object_key, kUploadUrlTtlSec);
    if (upload_url.empty()) {
        return std::nullopt;
    }

    const auto video_id = InsertUploadingVideo(request, object_key);
    if (!video_id) {
        return std::nullopt;
    }

    UploadSession session{};
    session.videoId = *video_id;
    session.objectKey = object_key;
    session.uploadUrl = upload_url;
    session.requestId = request.requestId;
    session.expiresIn = kUploadUrlTtlSec;
    session.fileName = request.fileName;
    return session;
}

CompleteUploadResult CompleteUpload(const CompleteUploadRequest& request) {
    if (request.uid == 0 || request.requestId.empty() || request.objectKey.empty()) {
        return {ErrorCodes::Error_Json, "invalid upload completion request"};
    }

    auto mysql = MySqlMgr::GetInstance();
    const auto video = mysql->GetUploadingVideo(request.uid, request.requestId);
    if (!video) {
        return {ErrorCodes::VideoNotFound, "video record not found"};
    }
    if (request.videoId != 0 && request.videoId != video->video_id) {
        return {ErrorCodes::UploadObjectMismatch, "video id mismatch"};
    }
    if (video->object_key != request.objectKey) {
        return {ErrorCodes::UploadObjectMismatch, "object key mismatch"};
    }
    if (video->status == 2 || video->status == 3) {
        return {ErrorCodes::Success, "upload already completed"};
    }
    if (video->status != 1) {
        return {ErrorCodes::UploadIncomplete, "video is not uploading"};
    }

    const auto actual_size = MinioClient::Instance().HeadObjectSize(request.objectKey);
    if (!actual_size) {
        return {ErrorCodes::UploadIncomplete, "uploaded object not found"};
    }
    if (video->file_size > 0 && *actual_size != video->file_size) {
        return {ErrorCodes::UploadIncomplete, "uploaded object size mismatch"};
    }
    if (!mysql->MarkVideoTranscoding(video->video_id, request.uid, *actual_size)) {
        return {ErrorCodes::DBError, "failed to update video status"};
    }

    return {ErrorCodes::Success, "upload completed"};
}

AdmitResponse UpLoadVideoServer::Admit(const RequestVideoData& request, const std::string& client_ip) {
    if (client_ip.empty()) {
        return {ErrorCodes::IPUnknown};
    }
    if (request.uid == 0 || request.requestId.empty()) {
        return {ErrorCodes::Error_Json};
    }
    if (request.title.empty() || request.title.size() > kUploadVideoMaxTitleLen) {
        return {ErrorCodes::Error_Json};
    }
    if (request.description.size() > kUploadVideoMaxDescLen) {
        return {ErrorCodes::Error_Json};
    }
    if (request.fileSize <= 0 || request.fileSize > kUploadVideoMaxBytes) {
        return {ErrorCodes::Error_Json};
    }
    if (request.tags.size() > kUploadVideoMaxTags) {
        return {ErrorCodes::Error_Json};
    }

    auto redis = RedisMgr::GetInstance();
    if (!redis->IsValid()) {
        return {ErrorCodes::VarifyBackErr};
    }

    const std::vector<redis::RateRule> rules{
            {UploadVideoUserPerMinute + std::to_string(request.uid), kUploadVideoUserPerMinute, std::chrono::seconds(60)},
            {UploadVideoIpPerMinute + client_ip, kUploadVideoIpPerMinute, std::chrono::seconds(60)}
    };

    const int hit = redis::RateLimiter::Acquire(*redis, rules);
    if (hit < 0) {
        return {ErrorCodes::VarifyBackErr};
    }
    if (hit > 0) {
        return {ErrorCodes::TooManyRequests};
    }

    const std::string ticket_key = std::string(UploadTicketKey) + request.requestId;
    const auto added = redis->SetNx(ticket_key, BuildTicketPayload(request, client_ip),
                                    std::chrono::seconds(kUploadTicketTtlSec));
    if (added == LimiterResult::LimiterError) {
        return {ErrorCodes::VarifyBackErr};
    }
    if (added == LimiterResult::LimiterDenied) {
        return {ErrorCodes::UploadDuplicated};
    }

    return {ErrorCodes::Success};
}

}
