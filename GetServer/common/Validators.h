#ifndef GETSERVER_COMMON_VALIDATORS_H
#define GETSERVER_COMMON_VALIDATORS_H

#include <string>

namespace validators {

    std::string TrimAscii(std::string value);

    std::string LowerAscii(std::string value);

    std::string NormalizeEmail(std::string value);

    bool IsValidEmail(const std::string& value);

    bool IsValidAccount(const std::string& value);

    bool IsValidUsername(const std::string& value);

    bool IsValidPassword(const std::string& value);

    bool IsValidPhone(const std::string& value);

    bool IsValidCode(const std::string& value);

} // namespace validators

#endif // GETSERVER_COMMON_VALIDATORS_H
