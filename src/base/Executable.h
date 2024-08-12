//
// Created by RedbeanW on 2024/2/4.
//

#pragma once

#include "base/Base.h"
#include "base/Loader.h"

#include "Loader.h"

#include <LIEF/Abstract/Binary.hpp>
#include <LIEF/Abstract/Symbol.hpp>

METADUMPER_BEGIN

class Executable : public Loader {
public:
    explicit Executable(const std::string& pPath) : Loader(pPath) {};
    virtual ~Executable() = default;

    [[nodiscard]] virtual uintptr_t getEndOfSections() const                                        = 0;
    [[nodiscard]] virtual size_t    getGapInFront(uintptr_t pAddr) const                            = 0;
    [[nodiscard]] virtual bool      isInSection(uintptr_t pAddr, const std::string& pSecName) const = 0;

    // lief's get_symbol is very slow!
    virtual LIEF::Symbol* lookupSymbol(uintptr_t pAddr)          = 0;
    virtual LIEF::Symbol* lookupSymbol(const std::string& pName) = 0;

    virtual bool moveToSection(const std::string& pName) = 0;

    ptrdiff_t getReadOffset(uintptr_t pAddr) override { return -(ptrdiff_t)getGapInFront(pAddr); }

    virtual LIEF::Binary* getImage() = 0;
};

METADUMPER_END
