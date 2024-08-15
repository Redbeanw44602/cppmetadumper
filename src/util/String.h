#pragma once

#include <string>

constexpr std::string StringRemovePrefix(std::string str, const std::string& prefix) {
    if (str.length() >= prefix.length() && str.substr(0, prefix.length()) == prefix) {
        return str.substr(prefix.length());
    }
    return str;
}