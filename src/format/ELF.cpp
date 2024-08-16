#include "ELF.h"

#include <magic_enum.hpp>

// #define DEBUG_DUMP_SECTION
#ifdef DEBUG_DUMP_SECTION
#include <fstream>
#endif

METADUMPER_FORMAT_BEGIN

ELF::ELF(const std::string& pPath) : Executable(pPath) {
    mImage = LIEF::ELF::Parser::parse(pPath);
    if (!mImage) {
        spdlog::error("Failed to load elf image.");
        mIsValid = false;
        return;
    }
    spdlog::info(
        "{:<12}{} for {}",
        "Format:",
        magic_enum::enum_name(mImage->type()),
        magic_enum::enum_name(mImage->header().machine_type())
    );
    _buildSymbolCache();
    _relocateReadonlyData();
}

uintptr_t ELF::getEndOfSections() const {
    uintptr_t ret = 0;
    for (auto& sec : mImage->sections()) {
        auto end = sec.virtual_address() + sec.size();
        if (ret < end) {
            ret = end;
        }
    }
    return ret;
}

size_t ELF::getGapInFront(uintptr_t pVAddr) const {
    size_t ret;
    for (auto& segment : mImage->segments()) {
        if (!segment.is_load()) continue;
        auto begin = segment.virtual_address();
        ret        = begin - segment.file_offset();
        if (pVAddr >= begin && pVAddr < begin + segment.virtual_size()) {
            return ret - getImageBase();
        }
    }
    throw std::runtime_error("An exception occurred during gap calculation!");
}

LIEF::ELF::Symbol* ELF::lookupSymbol(uintptr_t pVAddr) {
    if (mSymbolCache.mFromValue.contains(pVAddr)) return mSymbolCache.mFromValue.at(pVAddr);
    if (mDynSymbolCache.mFromValue.contains(pVAddr)) return mDynSymbolCache.mFromValue.at(pVAddr);
    return nullptr;
}

LIEF::ELF::Symbol* ELF::lookupSymbol(const std::string& pName) {
    if (mSymbolCache.mFromName.contains(pName)) return mSymbolCache.mFromName.at(pName);
    if (mDynSymbolCache.mFromName.contains(pName)) return mDynSymbolCache.mFromName.at(pName);
    return nullptr;
}

size_t ELF::getDynSymbolIndex(const std::string& pName) {
    return mDynSymbolIndexCache.contains(pName) ? mDynSymbolIndexCache.at(pName) : 0;
}

void ELF::_relocateReadonlyData() {
    // Reference:
    // https://github.com/ARM-software/abi-aa/releases/download/2023Q1/aaelf64.pdf
    // https://refspecs.linuxfoundation.org/elf/elf.pdf

    if (!mImage->has_section(".data.rel.ro")) return;

    const auto EOS = getEndOfSections();

    for (auto& relocation : mImage->dynamic_relocations()) {
        auto address = relocation.address();
        if (!isInSection(address, ".data.rel.ro")) continue;
        auto offset = address - getGapInFront(address);
        auto type   = relocation.type();
        using RELOC = LIEF::ELF::Relocation::TYPE;
        switch (type) {
        case RELOC::X86_64_64:
        case RELOC::AARCH64_ABS64: {
            auto symbol = relocation.symbol();
            if (symbol) {
                if (symbol->value()) {
                    // Internal Symbol
                    write<uintptr_t, false>(offset, symbol->value() + relocation.addend());
                } else {
                    // External Symbol
                    // fixme: Deviations may occur, although this does not affect data export.
                    write<uintptr_t, false>(
                        offset,
                        EOS + getDynSymbolIndex(symbol->name()) * sizeof(intptr_t) + relocation.addend()
                    );
                }
            } else {
                spdlog::error("Get dynamic symbol failed!");
            }
            break;
        }
        case RELOC::X86_64_RELATIVE:
        case RELOC::AARCH64_RELATIVE: {
            if (relocation.has_symbol()) {
                if (!relocation.addend()) {
                    spdlog::warn("Unknown type of ADDEND detected.");
                }
                write<uintptr_t, false>(offset, relocation.addend());
            } else {
                // External
                spdlog::warn("Unhandled type of RELATIVE detected.");
            }
            break;
        }
        default:
            spdlog::warn("Unhandled relocation type: {:#x}.", (uint32_t)type);
            break;
        }
    }
#ifdef DEBUG_DUMP_SECTION
    std::ofstream d_Dumper("relro.fixed.dump", std::ios::binary | std::ios::trunc);
    if (d_Dumper.is_open()) {
        move(begin, Begin);
        while (cur() < end) {
            auto data = read<unsigned char>();
            d_Dumper.write((char*)&data, sizeof(data));
        }
        d_Dumper.close();
    } else {
        spdlog::error("Failed to open file!");
    }
#endif
}

void ELF::_buildSymbolCache() {
    if (!mIsValid) return;

    if (mImage->has(LIEF::ELF::Section::TYPE::SYMTAB)) {
        for (auto& symbol : mImage->symtab_symbols()) {
            mSymbolCache.mFromName.try_emplace(symbol.name(), &symbol);
            mSymbolCache.mFromValue.try_emplace(symbol.value(), &symbol);
        }
    } else {
        spdlog::warn(".symtab not found in this image!");
    }

    if (mImage->has(LIEF::ELF::Section::TYPE::DYNSYM)) {
        auto idx = 0;
        for (auto& symbol : mImage->dynamic_symbols()) {
            mDynSymbolIndexCache.try_emplace(symbol.name(), idx);
            mDynSymbolCache.mFromName.try_emplace(symbol.name(), &symbol);
            mDynSymbolCache.mFromValue.try_emplace(
                getEndOfSections() + sizeof(intptr_t) * getDynSymbolIndex(symbol.name()),
                &symbol
            );
            idx++;
        }
    } else {
        spdlog::warn(".dynsym not found in this image!");
    }
}

METADUMPER_FORMAT_END
