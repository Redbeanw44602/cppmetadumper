//
// Created by RedbeanW on 2024/2/4.
//

#pragma once

#include "base/Base.h"
#include "base/Executable.h"

#include <LIEF/ELF.hpp>

METADUMPER_FORMAT_BEGIN

class ELF : public Executable {
public:
    explicit ELF(const std::string& pPath);

    [[nodiscard]] uintptr_t getEndOfSections() const override;
    [[nodiscard]] size_t    getGapInFront(uintptr_t pVAddr) const override;

    // lief's get_symbol is very slow!
    LIEF::ELF::Symbol* lookupSymbol(uintptr_t pVAddr) override;
    LIEF::ELF::Symbol* lookupSymbol(const std::string& pName) override;

    LIEF::ELF::Binary* getImage() const override { return mImage.get(); }

private:
    void _relocateReadonlyData();
    void _buildSymbolCache();

    struct SymbolCache {
        std::unordered_map<uintptr_t, LIEF::ELF::Symbol*>   mFromValue;
        std::unordered_map<std::string, LIEF::ELF::Symbol*> mFromName;
    };

    std::unique_ptr<LIEF::ELF::Binary> mImage;

    SymbolCache mSymbolCache;
    SymbolCache mDynSymbolCache;
};

METADUMPER_FORMAT_END
