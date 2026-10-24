//
// Chat login business logic.
//

#ifndef CHATSERVER1_LOGINSERVICE_H
#define CHATSERVER1_LOGINSERVICE_H

#include <memory>
#include <string>

class CSession;

namespace service {

class LoginService {
public:
    static void Handle(const std::shared_ptr<CSession>& session, const std::string& msg_data);
};

}

#endif  // CHATSERVER1_LOGINSERVICE_H
