#include "security/PasswordUtil.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <charconv>
#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

namespace security {
namespace {

constexpr int kHashIterations = 120000;
constexpr int kSaltBytes = 16;
constexpr int kDigestBytes = 32;
constexpr const char* kHashPrefix = "pbkdf2_sha256$";

std::string Base64Encode(const unsigned char* data, int length) {
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(4 * ((length + 2) / 3)), '\0');
    const int written = EVP_EncodeBlock(
            reinterpret_cast<unsigned char*>(result.data()), data, length);
    if (written < 0) {
        return {};
    }
    result.resize(static_cast<std::size_t>(written));
    return result;
}

std::vector<unsigned char> Base64Decode(const std::string& value) {
    if (value.empty() || value.size() % 4 != 0) {
        return {};
    }
    std::vector<unsigned char> result((value.size() / 4) * 3);
    const int written = EVP_DecodeBlock(
            result.data(), reinterpret_cast<const unsigned char*>(value.data()),
            static_cast<int>(value.size()));
    if (written < 0) {
        return {};
    }
    std::size_t padding = 0;
    if (!value.empty() && value.back() == '=') {
        ++padding;
    }
    if (value.size() >= 2 && value[value.size() - 2] == '=') {
        ++padding;
    }
    result.resize(static_cast<std::size_t>(written) - padding);
    return result;
}

} // namespace

bool PasswordUtil::Hash(const std::string& password, std::string& encoded) {
    unsigned char salt[kSaltBytes];
    unsigned char digest[kDigestBytes];
    if (RAND_bytes(salt, static_cast<int>(sizeof(salt))) != 1) {
        return false;
    }
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt, static_cast<int>(sizeof(salt)), kHashIterations,
                          EVP_sha256(), static_cast<int>(sizeof(digest)), digest) != 1) {
        return false;
    }

    encoded = kHashPrefix + std::to_string(kHashIterations)
            + "$" + Base64Encode(salt, static_cast<int>(sizeof(salt)))
            + "$" + Base64Encode(digest, static_cast<int>(sizeof(digest)));
    return true;
}

bool PasswordUtil::Verify(const std::string& password, const std::string& encoded) {
    const std::string prefix(kHashPrefix);
    if (encoded.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::size_t iterations_begin = prefix.size();
    const std::size_t iterations_end = encoded.find('$', iterations_begin);
    if (iterations_end == std::string::npos) {
        return false;
    }
    const std::size_t salt_end = encoded.find('$', iterations_end + 1);
    if (salt_end == std::string::npos) {
        return false;
    }

    int iterations = 0;
    const std::string iterations_text = encoded.substr(iterations_begin,
                                                       iterations_end - iterations_begin);
    const auto parse_result = std::from_chars(iterations_text.data(),
                                              iterations_text.data() + iterations_text.size(),
                                              iterations);
    if (parse_result.ec != std::errc() ||
        parse_result.ptr != iterations_text.data() + iterations_text.size() ||
        iterations < 1000 || iterations > 1000000) {
        return false;
    }

    const auto salt = Base64Decode(encoded.substr(iterations_end + 1,
                                                  salt_end - iterations_end - 1));
    const auto expected = Base64Decode(encoded.substr(salt_end + 1));
    if (salt.empty() || expected.empty()) {
        return false;
    }

    std::vector<unsigned char> actual(expected.size());
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()), iterations,
                          EVP_sha256(), static_cast<int>(actual.size()), actual.data()) != 1) {
        return false;
    }
    return actual.size() == expected.size() &&
           CRYPTO_memcmp(actual.data(), expected.data(), actual.size()) == 0;
}

} // namespace security
