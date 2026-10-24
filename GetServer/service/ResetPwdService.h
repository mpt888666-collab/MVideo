//
// Created by mpt on 2026/10/18.
//

#ifndef GETSERVER_RESETPWDSERVICE_H
#define GETSERVER_RESETPWDSERVICE_H

#include <string>

namespace service {
struct ResetPwdRequest {
    std::string email;
    std::string code;
    std::string new_password;
    std::string confirm;
};

struct ResetPwdResponse {
    int ec;
    int retry_after;
};

class ResetPwdService {
public:
    static ResetPwdResponse Admit(
            const ResetPwdRequest& request, const std::string& client_ip, ResetPwdRequest& normalized
    );

    static ResetPwdResponse ResetPwd(const ResetPwdRequest& normalized);
};
};  // namespace service
#endif  // GETSERVER_RESETPWDSERVICE_H
