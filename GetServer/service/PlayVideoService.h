//
// Created by mpt on 2026/11/20.
//

#ifndef GETSERVER_PLAYVIDEOSERVICE_H
#define GETSERVER_PLAYVIDEOSERVICE_H

#include <cstdint>
#include <string>

namespace service {

struct PlayVideoRequest {
    uint64_t userId{0};
    uint64_t videoId{0};
    std::string token;
};

struct PlayVideoData {
    uint64_t videoId{0};
    std::string title;
    std::string description;
    std::string coverUrl;
    std::string masterUrl;
    int expiresIn{0};
    uint64_t likeCount{0};
    uint64_t favoriteCount{0};
    uint64_t playCount{0};
    uint64_t commentCount{0};
};

class PlayVideoService {
public:
    static int Admit(const PlayVideoRequest& request, const std::string& ip);

    static const char* ErrorMessage(int error);
};

} // namespace service

#endif //GETSERVER_PLAYVIDEOSERVICE_H