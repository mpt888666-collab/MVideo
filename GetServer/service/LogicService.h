//
// Created by mpt on 2026/10/18.
//

#ifndef GETSERVER_LOGICSERVICE_H
#define GETSERVER_LOGICSERVICE_H
#include <functional>
#include <memory>
#include <string>

class HttpConnection;

namespace service {
struct LogicRequest {
    std::string email{};
    std::string password{};
    std::string request_id{};
};

struct LogicResponse {
    int ec = 0;
    int retry_after = 0;
    unsigned long long user_id = 0;
    std::string host{};
    std::string port{};
    std::string token{};
};

class LogicService {
public:
    using LogicDone = std::function<void(const service::LogicResponse&)>;

    static LogicResponse Admit(const LogicRequest& request, const std::string& client_ip, LogicRequest& normalized);

    static void Logic(const LogicRequest& normalized, std::shared_ptr<HttpConnection> connection, LogicDone done);
};
}  // namespace service
#endif  // GETSERVER_LOGICSERVICE_H
