#pragma once

#include "base/Base.h"

#include <string>

METADUMPER_UTIL_STRING_BEGIN

constexpr std::string remove_prefix(std::string str, const std::string& prefix) {
    if (str.length() >= prefix.length() && str.substr(0, prefix.length()) == prefix) {
        return str.substr(prefix.length());
    }
    return str;
}

constexpr unsigned int H(const char* str, int h = 0) { return !str[h] ? 5381 : (H(str, h + 1) * 33) ^ str[h]; }

METADUMPER_UTIL_STRING_END
