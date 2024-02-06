//
// Created by RedbeanW on 2024/1/19.
//

#include "Reader.h"

#include <fstream>

Reader::Reader(const std::string& pPath) {
    std::ifstream file;
    file.open(pPath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file.");
        mIsValid = false;
        return;
    }
    file.seekg(0, std::ios::beg); // ...what the fucking
    mStream << file.rdbuf();
    file.close();
}

bool Reader::isValid() const {
    return mIsValid;
}

std::string Reader::readCString(size_t pMaxLength) {
    std::string result;
    for (size_t i = 0; i < pMaxLength; i++) {
        auto chr = read<char>();
        if (chr == '\0') {
            break;
        }
        result += chr;
    }
    return result;
}

std::string Reader::readCString(uintptr_t pAddr, size_t pMaxLength) {
    auto after = cur();
    reset();
    move(pAddr, Begin);
    auto result = readCString(pMaxLength);
    move(after, Begin);
    return result;
}
