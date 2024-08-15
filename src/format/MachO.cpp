//
// Created by RedbeanW on 2024/2/4.
//

#include "MachO.h"

// magic_enum is out-of-range.

using MACHO_TYPES = LIEF::MachO::MACHO_TYPES;
using CPU_TYPE    = LIEF::MachO::Header::CPU_TYPE;

std::string macho_type_to_str(MACHO_TYPES type) {
    switch (type) {
    case MACHO_TYPES::MH_MAGIC:
    case MACHO_TYPES::MH_CIGAM:
    case MACHO_TYPES::MH_MAGIC_64:
    case MACHO_TYPES::MH_CIGAM_64:
        return "Mach-O";
    case MACHO_TYPES::FAT_MAGIC:
    case MACHO_TYPES::FAT_CIGAM:
        return "Mach-O Universal Binary";
    case MACHO_TYPES::NEURAL_MODEL:
        return "Mach-O Neural Network Model";
    case MACHO_TYPES::UNKNOWN:
    default:
        return "Unknown";
    }
}

std::string macho_cpu_to_str(CPU_TYPE cpu) {
    switch (cpu) {
    case CPU_TYPE::ANY:
        return "Any";
    case CPU_TYPE::X86:
        return "x86";
    case CPU_TYPE::X86_64:
        return "x86_64";
    case CPU_TYPE::MIPS:
        return "MIPS";
    case CPU_TYPE::MC98000:
        return "MC98000";
    case CPU_TYPE::ARM:
        return "ARM";
    case CPU_TYPE::ARM64:
        return "ARM64";
    case CPU_TYPE::SPARC:
        return "SPARC";
    case CPU_TYPE::POWERPC:
        return "PowerPC";
    case CPU_TYPE::POWERPC64:
        return "PowerPC64";
    default:
        return "Unknown";
    }
}

METADUMPER_FORMAT_BEGIN

MachO::MachO(const std::string& pPath) : Executable(pPath) {
    auto fatBinary = LIEF::MachO::Parser::parse(pPath);
    if (!fatBinary) {
        spdlog::error("Failed to load mach-o image.");
        mIsValid = false;
        return;
    }
    if (fatBinary->size() > 1) {
        spdlog::error("FatBinary are not supported yet.");
        mIsValid = false;
        return;
    }
    mImage     = fatBinary->take(0);
    auto magic = mImage->header().magic();
    if (magic != MACHO_TYPES::MH_MAGIC_64) {
        spdlog::error("{} are not supported yet.", macho_type_to_str(magic));
        return;
    }
    spdlog::info("{:<12}{} for {}", "Format:", macho_type_to_str(magic), macho_cpu_to_str(mImage->header().cpu_type()));
    _buildSymbolCache();
}

uintptr_t MachO::getEndOfSections() const {
    uintptr_t ret = 0;
    for (auto& sec : mImage->sections()) {
        auto end = sec.virtual_address() + sec.size();
        if (ret < end) {
            ret = end;
        }
    }
    return ret;
}

size_t MachO::getGapInFront(uintptr_t pVAddr) const {
    size_t ret;
    for (auto& segment : mImage->segments()) {
        auto begin = segment.virtual_address();
        ret        = begin - segment.file_offset();
        if (pVAddr >= begin && pVAddr < begin + segment.virtual_size()) {
            return ret - getImageBase();
        }
    }
    throw std::runtime_error("An exception occurred during gap calculation!");
}

LIEF::MachO::Symbol* MachO::lookupSymbol(uintptr_t pVAddr) {
    if (mSymbolCache.mFromValue.contains(pVAddr)) return mSymbolCache.mFromValue.at(pVAddr);
    return nullptr;
}

LIEF::MachO::Symbol* MachO::lookupSymbol(const std::string& pName) {
    if (mSymbolCache.mFromName.contains(pName)) return mSymbolCache.mFromName.at(pName);
    return nullptr;
}

void MachO::_buildSymbolCache() {
    if (!mIsValid) return;

    if (!mImage->has_section("__symtab")) {
        spdlog::warn("__symtab not found in this image!");
    }

    for (auto& symbol : mImage->symbols()) {
        mSymbolCache.mFromName.try_emplace(symbol.name(), &symbol);
        mSymbolCache.mFromValue.try_emplace(symbol.value(), &symbol);
    }
}

METADUMPER_FORMAT_END
