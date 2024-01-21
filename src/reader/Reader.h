//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "Base.h"

#include <elfio/elfio.hpp>

using ELFIO::Elf64_Addr;
using ELFIO::Elf_Word;
using ELFIO::Elf_Xword;
using ELFIO::Elf_Half;
using ELFIO::Elf_Sxword;

#define READER_UNREACHABLE

enum class Architecture {
    Unsupported,
    X86_64,
    AArch64
};

struct ReaderState {
    bool mIsValid { true };
    std::string mMessage;
    void setError(std::string pMessage) {
        mIsValid = false;
        mMessage = std::move(pMessage);
    }
    explicit operator bool() const {
        return mIsValid;
    }
};

struct Symbol {
    std::string     mName;
    Elf64_Addr      mValue;
    Elf_Xword       mSize;
    unsigned char   mBind;
    unsigned char   mType;
    Elf_Half        mSectionIndex;
    unsigned char   mOther;
};

struct Relocation {
    Elf64_Addr      mOffset;
    Elf64_Addr      mSymbolValue;
    std::string     mSymbolName;
    unsigned        mType;
    Elf_Sxword      mAddend;
    Elf_Sxword      mCalcValue;
};

class Reader {
public:

    explicit Reader(const std::string& pPath);

    [[nodiscard]] ReaderState getState() const;

protected:

    [[nodiscard]] Elf64_Addr getEndOfSections() const;
    [[nodiscard]] Architecture getArchitecture() const;
    [[nodiscard]] uint64_t getGapInFront(Elf64_Addr pAddr) const;

    enum RelativePos { Begin, Current, End };

    template <typename T>
    [[nodiscard]] T read() {
        unsigned char buffer[sizeof(T)];
        mStream.read((char*)buffer, sizeof(T));
        return FromBytes<T>(buffer);
    };

    template <typename T, bool KeepOriPos>
    [[nodiscard]] T read(Elf64_Addr pBegin) {
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
    }

    template <typename T, bool KeepOriPos>
    void write(Elf64_Addr pBegin, T pData) {
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
    std::string readCString(Elf64_Addr pAddr, size_t pMaxLength);

    bool forEachSymbols(ELFIO::section* pSec, const std::function<void(Symbol)>& pCall);
    bool forEachSymbols(const std::function<void(Symbol)>& pCall);

    bool forEachRelocations(const std::function<void(Relocation)>& pCall);

    std::optional<Symbol> lookupSymbol(Elf64_Addr pAddr);
    std::optional<Symbol> lookupSymbol(const std::string& pName);

    inline Elf64_Addr cur() {
        return mStream.tellg();
    }

    inline bool move(Elf64_Addr pPos, RelativePos pRel = Current) {
        mStream.seekp(pPos, (int)pRel);
        mStream.seekg(pPos, (int)pRel);
        return mStream.good();
    }

    inline void reset() {
        mStream.clear();
    }

    ReaderState mState;

private:

    ELFIO::section* _fetchSection(const char* pSecName);

    void _relocateReadonlyData();

    std::stringstream mStream;
    ELFIO::elfio mImage;

};