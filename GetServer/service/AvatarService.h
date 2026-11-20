#ifndef GETSERVER_AVATARSERVICE_H
#define GETSERVER_AVATARSERVICE_H

#include <string>

namespace service {

struct AvatarTicketRequest {
    unsigned long long uid{0};
    std::string token;
    std::string requestId;
    unsigned long long fileSize{0};
};

struct AvatarTicketResponse {
    int ec{0};
    std::string objectKey;
    std::string uploadUrl;
    std::string message;
    int expiresIn{0};
};

struct AvatarCompleteResponse {
    int ec{0};
    std::string avatarKey;
    std::string avatarUrl;
    std::string message;
};

class AvatarService {
public:
    static AvatarTicketResponse CreateTicket(
            const AvatarTicketRequest& request, const std::string& clientIp
    );

    static AvatarCompleteResponse Complete(
            unsigned long long uid, const std::string& token,
            const std::string& objectKey, const std::string& clientIp
    );
};

}  // namespace service

#endif  // GETSERVER_AVATARSERVICE_H