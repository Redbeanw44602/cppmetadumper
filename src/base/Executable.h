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

    [[nodiscard]] virtual uintptr_t getEndOfSections() const = 0;
    [[nodiscard]] virtual bool      isInSection(uintptr_t pVAddr, const std::string& pSecName) const;
    [[nodiscard]] intptr_t          getImageBase() const override { return getImage()->imagebase(); };

    // lief's get_symbol is very slow!
    virtual LIEF::Symbol* lookupSymbol(uintptr_t pVAddr)         = 0;
    virtual LIEF::Symbol* lookupSymbol(const std::string& pName) = 0;

    virtual LIEF::Binary* getImage() const = 0;
};

METADUMPER_END
