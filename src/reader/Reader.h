//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "Base.h"

#define READER_UNREACHABLE

class Reader {
public:
    explicit Reader(const std::string& pPath);

    [[nodiscard]] bool isValid() const;

protected:
    enum RelativePos { Begin, Current, End };

    virtual ptrdiff_t getReadOffset(uintptr_t pAddr) { return 0; };

    template <typename T>
    [[nodiscard]] T read() {
        T    value;
        auto off = getReadOffset(cur());
        move(off);
        mStream.read((char*)&value, sizeof(T));
        move(-off);
        mLastOperated = sizeof(T);
        return value;
    };

    template <typename T, bool KeepOriPos>
    [[nodiscard]] T read(uintptr_t pBegin) {
        if constexpr (KeepOriPos) {
            auto after = cur();
            reset();
            auto ret = read<T, false>(pBegin);
            move(after, Begin);
            return ret;
        }
        move(pBegin, Begin);
        return read<T>();
    }

    template <typename T>
    void write(T pData) {
        mStream.write(reinterpret_cast<char*>(&pData), sizeof(T));
        mLastOperated = sizeof(T);
    }

    template <typename T, bool KeepOriPos>
    void write(uintptr_t pBegin, T pData) {
        if constexpr (KeepOriPos) {
            auto after = cur();
            reset();
            write<T, false>(pBegin, pData);
            move(after, Begin);
            return;
        }
        move(pBegin, Begin);
        write<T>(pData);
    }

    std::string readCString(size_t pMaxLength);
    std::string readCString(uintptr_t pAddr, size_t pMaxLength);

    inline uintptr_t cur() { return mStream.tellg(); }

    inline uintptr_t last() { return cur() - mLastOperated; }

    inline bool move(int64_t pPos, RelativePos pRel = Current) {
        mStream.seekp(pPos, (std::ios_base::seekdir)pRel);
        mStream.seekg(pPos, (std::ios_base::seekdir)pRel);
        return mStream.good();
    }

    inline void reset() { mStream.clear(); }

    bool mIsValid{true};

private:
    std::stringstream mStream;

    uint64_t mLastOperated{0};
};