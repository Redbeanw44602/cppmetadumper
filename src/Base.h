//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

constexpr unsigned int H(const char* str, int h = 0) {
    return !str[h] ? 5381 : (H(str, h + 1) * 33) ^ str[h];
}

template<typename T>
constexpr T FromBytes(const unsigned char* seq) {
    T value;
    std::memcpy(&value, seq, sizeof(T));
    return value;
}
