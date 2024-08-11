//
// Created by RedbeanW on 2024/2/4.
//

#pragma once

#include "base/Base.h"
#include "base/Loader.h"

#include <elfio/elfio.hpp>

METADUMPER_ELF_BEGIN

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
    std::string       mName;
    ELFIO::Elf64_Addr mValue;
    ELFIO::Elf_Xword  mSize;
    unsigned char     mBind;
    unsigned char     mType;
    ELFIO::Elf_Half   mSectionIndex;
    unsigned char     mOther;
};

struct Relocation {
    ELFIO::Elf64_Addr mOffset;
    ELFIO::Elf_Word   mSymbolIndex;
    unsigned          mType;
    ELFIO::Elf_Sxword mAddend;
};

class ELF : public Loader {
public:
    explicit ELF(const std::string& pPath);

    [[nodiscard]] Architecture getArchitecture() const;

protected:
    [[nodiscard]] uintptr_t getEndOfSections() const;
    [[nodiscard]] size_t    getGapInFront(uintptr_t pAddr) const;
    [[nodiscard]] bool      isInSection(uintptr_t pAddr, const std::string& pSecName) const;

    bool forEachSymbolTable(ELFIO::section* pSec, const std::function<void(size_t, Symbol)>& pCall);

    bool forEachSymbols(const std::function<void(size_t, Symbol)>& pCall);
    bool forEachDynSymbols(const std::function<void(size_t, Symbol)>& pCall);

    bool forEachRelocations(const std::function<void(Relocation)>& pCall);

    std::optional<Symbol> lookupSymbol(uintptr_t pAddr);
    std::optional<Symbol> lookupSymbol(const std::string& pName);

    std::optional<Symbol> getSymbol(ELFIO::section* pSec, size_t pIndex);
    std::optional<Symbol> getSymbol(uintptr_t pIndex);

    std::optional<Symbol> getDynSymbol(uintptr_t pIndex);

    bool moveToSection(const std::string& pName);

    ptrdiff_t getReadOffset(uintptr_t pAddr) override { return -(ptrdiff_t)getGapInFront(pAddr); }

private:
    ELFIO::section* _fetchSection(const std::string& pSecName) const;

    void _relocateReadonlyData();
    void _buildSymbolCache();

    ELFIO::elfio mImage;

    struct SymbolCache {
        std::unordered_map<std::string, size_t> mFromName;
        std::unordered_map<uintptr_t, size_t>   mFromValue;
    };

    SymbolCache mSymbolCache;
    SymbolCache mDynSymbolCache;
};

METADUMPER_ELF_END
