//
// Created by mpt on 2026/10/27.
//

#ifndef GETSERVER_UPLOADVIDEOSERVER_H
#define GETSERVER_UPLOADVIDEOSERVER_H
#include <optional>
#include <string>
#include <vector>
#include <boost/beast/http/status.hpp>
#include "const.h"

namespace service {
    // 业务错误码 -> HTTP 状态码
    boost::beast::http::status HttpStatusForError(int error);

    struct ResponseVideoData {
        unsigned long long videoId{0};
        std::string objectKey;
        std::string uploadUrl;
        std::string requestId;
        std::string message;
        int expiresIn{0};
    };

    struct RequestVideoData {
        unsigned long long uid{0};
        std::string requestId;
        std::string title;
        std::string description;
        std::vector<std::string> tags;
        std::string fileName;
        double fileSize{0};
        std::string token;

    };

    struct AdmitResponse {
        int ec{ErrorCodes::Success};
    };

    struct UploadSession {
        unsigned long long videoId{0};
        std::string objectKey;
        std::string uploadUrl;
        std::string requestId;
        std::string fileName;
        int expiresIn{0};
    };

    struct CompleteUploadRequest {
        unsigned long long uid{0};
        unsigned long long videoId{0};
        std::string requestId;
        std::string objectKey;
    };

    struct CompleteUploadResult {
        int ec{ErrorCodes::Success};
        std::string message;
    };

    std::string MakeUploadObjectKey(const RequestVideoData& request);

    std::optional<unsigned long long> InsertUploadingVideo(
            const RequestVideoData& request, const std::string& object_key
    );

    std::optional<UploadSession> CreateUploadSession(const RequestVideoData& request);

    CompleteUploadResult CompleteUpload(const CompleteUploadRequest& request);

    class UpLoadVideoServer {
    public:
        static AdmitResponse Admit(const RequestVideoData& request, const std::string& client_ip);
    };
}


#endif //GETSERVER_UPLOADVIDEOSERVER_H
