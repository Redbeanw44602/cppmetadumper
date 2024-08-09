//
// Created by RedbeanW on 2024/2/4.
//

#pragma once

#include "Reader.h"

#include <elfio/elfio.hpp>

using ELFIO::Elf64_Addr;
using ELFIO::Elf_Half;
using ELFIO::Elf_Sxword;
using ELFIO::Elf_Word;
using ELFIO::Elf_Xword;

enum class Architecture { Unsupported, X86_64, AArch64 };

constexpr std::string arch2str(Architecture arch) {
    switch (arch) {
    case Architecture::X86_64:
        return "x86_64";
    case Architecture::AArch64:
        return "aarch64";
    case Architecture::Unsupported:
    default:
        return "unsupported";
    }
}

struct Symbol {
    std::string   mName;
    Elf64_Addr    mValue;
    Elf_Xword     mSize;
    unsigned char mBind;
    unsigned char mType;
    Elf_Half      mSectionIndex;
    unsigned char mOther;
};

struct Relocation {
    Elf64_Addr mOffset;
    Elf_Word   mSymbolIndex;
    unsigned   mType;
    Elf_Sxword mAddend;
};

class ElfReader : public Reader {
public:
    explicit ElfReader(const std::string& pPath);

    [[nodiscard]] Architecture getArchitecture() const;

protected:
    [[nodiscard]] Elf64_Addr getEndOfSections() const;
    [[nodiscard]] uint64_t   getGapInFront(Elf64_Addr pAddr) const;
    [[nodiscard]] bool       isInSection(Elf64_Addr pAddr, const char* pSecName) const;

    bool forEachSymbolTable(ELFIO::section* pSec, const std::function<void(uint64_t, Symbol)>& pCall);

    bool forEachSymbols(const std::function<void(uint64_t, Symbol)>& pCall);
    bool forEachDynSymbols(const std::function<void(uint64_t, Symbol)>& pCall);

    bool forEachRelocations(const std::function<void(Relocation)>& pCall);

    std::optional<Symbol> lookupSymbol(Elf64_Addr pAddr);
    std::optional<Symbol> lookupSymbol(const std::string& pName);

    std::optional<Symbol> getSymbol(ELFIO::section* pSec, uint64_t pIndex);
    std::optional<Symbol> getSymbol(uint64_t pIndex);

    std::optional<Symbol> getDynSymbol(uint64_t pIndex);

    ptrdiff_t getReadOffset(Elf64_Addr pAddr) override { return -(ptrdiff_t)getGapInFront(pAddr); }

private:
    ELFIO::section* _fetchSection(const char* pSecName) const;

    void _relocateReadonlyData();
    void _buildSymbolCache();

    ELFIO::elfio mImage;

    struct SymbolCache {
        std::unordered_map<std::string, uint64_t> mFromName;
        std::unordered_map<Elf64_Addr, uint64_t>  mFromValue;
    };

    SymbolCache mSymbolCache;
    SymbolCache mDynSymbolCache;
};