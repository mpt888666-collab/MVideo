#include "common/Validators.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <regex>
#include <utility>

namespace validators {
namespace {

bool IsAsciiSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool IsValidUtf8(const std::string& value, std::size_t& char_count) {
    char_count = 0;
    for (std::size_t i = 0; i < value.size();) {
        const auto first = static_cast<unsigned char>(value[i]);
        std::size_t length = 0;
        std::uint32_t code_point = 0;

        if (first < 0x80) {
            length = 1;
            code_point = first;
        } else if ((first & 0xE0) == 0xC0) {
            length = 2;
            code_point = first & 0x1F;
        } else if ((first & 0xF0) == 0xE0) {
            length = 3;
            code_point = first & 0x0F;
        } else if ((first & 0xF8) == 0xF0) {
            length = 4;
            code_point = first & 0x07;
        } else {
            return false;
        }

        if (i + length > value.size()) {
            return false;
        }
        for (std::size_t j = 1; j < length; ++j) {
            const auto next = static_cast<unsigned char>(value[i + j]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (next & 0x3F);
        }

        if ((length == 2 && code_point < 0x80) ||
            (length == 3 && code_point < 0x800) ||
            (length == 4 && code_point < 0x10000) ||
            code_point > 0x10FFFF ||
            (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            return false;
        }

        ++char_count;
        i += length;
    }
    return true;
}

} // namespace

std::string TrimAscii(std::string value) {
    std::size_t begin = 0;
    while (begin < value.size() && IsAsciiSpace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && IsAsciiSpace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string NormalizeEmail(std::string value) {
    return LowerAscii(TrimAscii(std::move(value)));
}

bool IsValidEmail(const std::string& value) {
    if (value.empty() || value.size() > 254) {
        return false;
    }
    static const std::regex pattern(
            R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
    return std::regex_match(value, pattern);
}

bool IsValidAccount(const std::string& value) {
    if (value.size() < 4 || value.size() > 32) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '_';
    });
}

bool IsValidUsername(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    std::size_t char_count = 0;
    if (!IsValidUtf8(value, char_count) || char_count > 64) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return c >= 0x20 && c != 0x7f;
    });
}

bool IsValidPassword(const std::string& value) {
    return value.size() >= 8 && value.size() <= 128;
}

bool IsValidPhone(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    if (value.size() < 7 || value.size() > 16) {
        return false;
    }
    const std::size_t begin = value.front() == '+' ? 1 : 0;
    if (begin == value.size()) {
        return false;
    }
    return std::all_of(value.begin() + static_cast<std::ptrdiff_t>(begin), value.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool IsValidCode(const std::string& value) {
    if (value.size() < 4 || value.size() > 8) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalnum(c) != 0;
    });
}

} // namespace validators
