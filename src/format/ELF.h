#pragma once

#include "base/Base.h"
#include "base/Executable.h"

// Don't use LIEF low performance method:
// * get_symbol() -> use ELF::lookupSymbol() instead
// * dynsym_idx() -> use ELF::getDynSymbolIndex() instead

#include <LIEF/ELF.hpp>

METADUMPER_FORMAT_BEGIN

class ELF : public Executable {
public:
    explicit ELF(const std::string& pPath);

    [[nodiscard]] uintptr_t getEndOfSections() const override;
    [[nodiscard]] size_t    getGapInFront(uintptr_t pVAddr) const override;

    LIEF::ELF::Symbol* lookupSymbol(uintptr_t pVAddr) override;
    LIEF::ELF::Symbol* lookupSymbol(const std::string& pName) override;

    size_t getDynSymbolIndex(const std::string& pName);

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

    std::unordered_map<std::string, size_t> mDynSymbolIndexCache;
};

METADUMPER_FORMAT_END
