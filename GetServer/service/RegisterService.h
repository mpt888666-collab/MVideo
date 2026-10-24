#ifndef GETSERVER_SERVICE_REGISTERSERVICE_H
#define GETSERVER_SERVICE_REGISTERSERVICE_H

#include <string>

namespace service {
struct RegisterRequest {
    std::string account;
    std::string password;
    std::string username;
    std::string email;
    std::string phone;
    std::string code;
};

struct RegisterResult {
    int error{0};
    long long user_id{0};
    int retry_after{0};
    std::string message;
};

class RegisterService {
public:
    static RegisterResult Admit(
            const RegisterRequest& request, const std::string& client_ip, RegisterRequest& normalized
    );

    static RegisterResult Register(const RegisterRequest& normalized);
};
}  // namespace service
#endif  // GETSERVER_SERVICE_REGISTERSERVICE_H
