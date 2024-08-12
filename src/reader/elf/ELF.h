//
// Created by RedbeanW on 2024/2/4.
//

#pragma once

#include "base/Base.h"
#include "base/Loader.h"

#include <LIEF/ELF.hpp>

METADUMPER_ELF_BEGIN

class ELF : public Loader {
public:
    explicit ELF(const std::string& pPath);

protected:
    [[nodiscard]] uintptr_t getEndOfSections() const;
    [[nodiscard]] size_t    getGapInFront(uintptr_t pAddr) const;
    [[nodiscard]] bool      isInSection(uintptr_t pAddr, const std::string& pSecName) const;

    // lief's get_symbol is very slow!
    LIEF::ELF::Symbol* lookupSymbol(uintptr_t pAddr);
    LIEF::ELF::Symbol* lookupSymbol(const std::string& pName);

    bool moveToSection(const std::string& pName);

    ptrdiff_t getReadOffset(uintptr_t pAddr) override { return -(ptrdiff_t)getGapInFront(pAddr); }

    std::unique_ptr<LIEF::ELF::Binary> mImage;

private:
    void _relocateReadonlyData();
    void _buildSymbolCache();

    struct SymbolCache {
        std::unordered_map<uintptr_t, LIEF::ELF::Symbol*>   mFromValue;
        std::unordered_map<std::string, LIEF::ELF::Symbol*> mFromName;
    };

    SymbolCache mSymbolCache;
    SymbolCache mDynSymbolCache;
};

METADUMPER_ELF_END
