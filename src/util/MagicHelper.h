#pragma once

#include "base/Base.h"
#include "base/Loader.h"

METADUMPER_BEGIN

enum class Magic {
    UNKNOWN,
    ELF,
    PE,
    MACHO_32,
    MACHO_64,
};

class MagicHelper : public Loader {
public:
    using Loader::Loader;

    Magic judgeFileType() {
        auto magic = read<uint32_t>(); // read 4 bytes
        switch (magic) {
        case 0x464c457f:
            return Magic::ELF;
        case 0xfeedface:
            return Magic::MACHO_32;
        case 0xfeedfacf:
            return Magic::MACHO_64;
        }
        if ((magic & 0xffff) == 0x5a4d) return Magic::PE;
        return Magic::UNKNOWN;
    };
};

METADUMPER_END
