//
// Created by RedbeanW on 2024/2/4.
//

#pragma once

#include "base/Base.h"
#include "base/Executable.h"

#include <LIEF/MachO.hpp>

METADUMPER_FORMAT_BEGIN

class MachO : public Executable {
public:
    explicit MachO(const std::string& pPath);

    [[nodiscard]] uintptr_t getEndOfSections() const override;
    [[nodiscard]] size_t    getGapInFront(uintptr_t pVAddr) const override;

    // lief's get_symbol is very slow!
    LIEF::MachO::Symbol* lookupSymbol(uintptr_t pVAddr) override;
    LIEF::MachO::Symbol* lookupSymbol(const std::string& pName) override;

    LIEF::MachO::Binary* getImage() const override { return mImage.get(); }

private:
    void _relocateReadonlyData();
    void _buildSymbolCache();

    struct SymbolCache {
        std::unordered_map<uintptr_t, LIEF::MachO::Symbol*>   mFromValue;
        std::unordered_map<std::string, LIEF::MachO::Symbol*> mFromName;
    };

    std::unique_ptr<LIEF::MachO::Binary> mImage;

    SymbolCache mSymbolCache;
};

METADUMPER_FORMAT_END
