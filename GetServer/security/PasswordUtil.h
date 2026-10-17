#ifndef GETSERVER_SECURITY_PASSWORDUTIL_H
#define GETSERVER_SECURITY_PASSWORDUTIL_H

#include <string>

namespace security {

class PasswordUtil {
public:
    static bool Hash(const std::string& password, std::string& encoded);

    static bool Verify(const std::string& password, const std::string& encoded);
};

} // namespace security

#endif // GETSERVER_SECURITY_PASSWORDUTIL_H
