//
// Created by RedbeanW on 2023/7/27.
//

#pragma once

#include <string>
#include <sstream>

static_assert(sizeof(uint64_t) == 8);
static_assert(sizeof(int64_t) == 8);
static_assert(sizeof(int32_t) == 4);

constexpr std::string UInt64ToByteSequence(unsigned long long num) {
    unsigned char bytes[8];
    for (int j = 0; j < 8; j++) {
        bytes[j] = (num >> (j * 8)) & 0xFF;
    }
    return { (char*)bytes, 8 };
}

inline bool IsAllBytesZero(std::stringstream& ss, std::streampos len, std::streampos beg = 0) {
    std::streampos cur = ss.tellg();
    ss.clear();
    ss.seekg(beg);
    char c;
    while (ss.get(c) && ss.tellg() < beg + len) {
        if (c != '\0') {
            ss.seekg(cur);
            return false;
        }
    }
    ss.seekg(cur);
    return true;
}

constexpr unsigned int H(const char* str, int h = 0) {
    return !str[h] ? 5381 : (H(str, h + 1) * 33) ^ str[h];
}

template<typename T>
constexpr T FromBytes(const unsigned char* seq) {
    T value;
    std::memcpy(&value, seq, sizeof(T));
    return value;
}
