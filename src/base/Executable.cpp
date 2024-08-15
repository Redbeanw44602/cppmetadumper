#include "Executable.h"

#include <LIEF/Abstract/Section.hpp>

METADUMPER_BEGIN

bool Executable::isInSection(uintptr_t pVAddr, const std::string& pSecName) const {
    // There may be a section with the same name, so you cannot use get_section(std::string const&) directly.
    for (auto& section : getImage()->sections()) {
        if (section.name() == pSecName && section.virtual_address() <= pVAddr
            && (section.virtual_address() + section.size()) > pVAddr) {
            return true;
        }
    }
    return false;
}

METADUMPER_END
