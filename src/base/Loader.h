//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "Base.h"

METADUMPER_BEGIN

enum RelativePos { Begin, Current, End };

class Loader {
public:
    explicit Loader(const std::string& pPath);
    virtual ~Loader() = default;

    [[nodiscard]] bool isValid() const;

    // Read

    template <typename T>
    [[nodiscard]] T read() {
        T    value;
        auto offset = getGapInFront(cur()); // Adjust file offset to fit in-memory position.
        move(-offset);
        mStream.read((char*)&value, sizeof(T));
        move(offset);
        mLastOperated = sizeof(T);
        return value;
    };

    template <typename T, bool KeepOriPos>
    [[nodiscard]] T read(uintptr_t pVAddr) {
        pVAddr -= getImageBase(); // Adjust the in-memory position to file-offset.
        if constexpr (KeepOriPos) {
            auto after = cur();
            auto ret   = read<T, false>(pVAddr);
            move(after, Begin);
            return ret;
        }
        move(pVAddr, Begin);
        return read<T>();
    }

    // Write

    // Temporarily disabled because the unaddressed write behavior is unclear.

    // template <typename T>
    // void write(T pData) {
    //     mStream.write(reinterpret_cast<char*>(&pData), sizeof(T));
    //     mLastOperated = sizeof(T);
    // }

    template <typename T, bool KeepOriginalPosition>
    void write(uintptr_t pVAddr, T pData) {
        if constexpr (KeepOriginalPosition) {
            auto after = cur();
            write<T, false>(pVAddr, pData);
            move(after, Begin);
            return;
        }
        move(pVAddr, Begin);
        // inlined from unaddressed write.
        mStream.write(reinterpret_cast<char*>(&pData), sizeof(T));
        mLastOperated = sizeof(T);
    }

    // Position

    inline uintptr_t cur() { return mStream.tellg() + getImageBase(); }

    inline uintptr_t last() { return cur() - mLastOperated; }

    // Temporarily disabled because the unsafe move is checked above.

    // inline void reset() { mStream.clear(); }

    inline bool move(intptr_t pVal, RelativePos pRel = Current) {
        if (pVal && pRel == Begin) pVal -= getImageBase();
        mStream.seekp(pVal, (std::ios_base::seekdir)pRel);
        mStream.seekg(pVal, (std::ios_base::seekdir)pRel);
        if (!mStream.good()) throw std::runtime_error("BinaryStream is broken.");
        return true;
    }

    // Utils

    std::string readCString(size_t pMaxLength);
    std::string readCString(uintptr_t pVAddr, size_t pMaxLength);

protected:
    bool mIsValid{true};

    virtual size_t getGapInFront(uintptr_t pVAddr) const { return 0; };

    virtual intptr_t getImageBase() const { return 0; };

private:
    std::stringstream mStream;

    size_t mLastOperated{};
};

METADUMPER_END
