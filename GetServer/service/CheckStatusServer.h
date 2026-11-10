//
// Created by mpt on 2026/11/10.
//

#ifndef GETSERVER_CHECKSTATUSSERVER_H
#define GETSERVER_CHECKSTATUSSERVER_H
#include <vector>
#include <string>
namespace service {
    struct Videos {
        unsigned long long videoId;
        int status;
    };
    struct ResponseData {
        unsigned long long userId;
        bool success{false};
        std::vector<Videos> videos;
    };

    struct RequestData {
        unsigned long long userId;
        std::string token;
        std::string ip;
    };

    class CheckStatusServer {
    public:
        static int Admit(const RequestData& data);
    };
}


#endif //GETSERVER_CHECKSTATUSSERVER_H