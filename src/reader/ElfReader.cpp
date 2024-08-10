//
// Created by RedbeanW on 2024/2/4.
//

#include "ElfReader.h"

using namespace ELFIO;

ElfReader::ElfReader(const std::string& pPath) : Reader(pPath) {
    if (!mImage.load(pPath)) { // todo: Find better solution.
        spdlog::error("Failed to load elf image.");
        mIsValid = false;
        return;
    }
    _buildSymbolCache();
    _relocateReadonlyData();
}

Elf64_Addr ElfReader::getEndOfSections() const {
    Elf64_Addr ret = 0;
    for (auto& sec : mImage.sections) {
        auto end = sec->get_address() + sec->get_size();
        if (ret < end) {
            ret = end;
        }
    }
    return ret;
}

Architecture ElfReader::getArchitecture() const {
    switch (mImage.get_machine()) {
    case EM_X86_64:
        return Architecture::X86_64;
    case EM_AARCH64:
        return Architecture::AArch64;
    default:
        return Architecture::Unsupported;
    }
}

uint64_t ElfReader::getGapInFront(Elf64_Addr pAddr) const {
    enum { LOAD = 1 };
    uint64_t gap;
    for (auto& seg : mImage.segments) {
        if (seg->get_type() != LOAD) {
            continue;
        }
        auto begin = seg->get_virtual_address();
        gap        = begin - seg->get_offset();
        if (pAddr >= begin && pAddr < begin + seg->get_memory_size()) {
            return gap;
        }
    }
    READER_UNREACHABLE
    return -1;
}

bool ElfReader::isInSection(Elf64_Addr pAddr, const std::string& pSecName) const {
    auto section = _fetchSection(pSecName);
    return section && section->get_address() < pAddr && (section->get_address() + section->get_size()) > pAddr;
}
bool ElfReader::forEachSymbolTable(ELFIO::section* pSec, const std::function<void(uint64_t, Symbol)>& pCall) {
    if (!pSec) return false;
    symbol_section_accessor accessor(mImage, pSec);
    if (accessor.get_symbols_num() <= 0) return false;
    for (Elf_Xword idx = 0; idx < accessor.get_symbols_num(); ++idx) {
        std::string   name;
        Elf64_Addr    value;
        Elf_Xword     size;
        unsigned char bind;
        unsigned char type;
        Elf_Half      sectionIndex;
        unsigned char other;
        if (accessor.get_symbol(idx, name, value, size, bind, type, sectionIndex, other)) {
            pCall(idx, Symbol{name, value, size, bind, type, sectionIndex, other});
        }
    }
    return true;
}

bool ElfReader::forEachSymbols(const std::function<void(uint64_t, Symbol)>& pCall) {
    return forEachSymbolTable(_fetchSection(".symtab"), pCall);
}

bool ElfReader::forEachDynSymbols(const std::function<void(uint64_t, Symbol)>& pCall) {
    return forEachSymbolTable(_fetchSection(".dynsym"), pCall);
}

bool ElfReader::forEachRelocations(const std::function<void(Relocation)>& pCall) {
    // todo: relaPlt
    section* relaDyn;
    if (!(relaDyn = _fetchSection(".rela.dyn"))) {
        return false;
    }
    const relocation_section_accessor accessor(mImage, relaDyn);
    for (Elf_Xword idx = 0; idx < accessor.get_entries_num(); ++idx) {
        Elf64_Addr offset{};
        Elf_Word   symbol{};
        unsigned   type{};
        Elf_Sxword addend{};
        if (accessor.get_entry(idx, offset, symbol, type, addend)) {
            pCall(Relocation{offset, symbol, type, addend});
        }
    }
    return true;
}

std::optional<Symbol> ElfReader::lookupSymbol(Elf64_Addr pAddr) {
    if (mSymbolCache.mFromValue.contains(pAddr)) return getSymbol(mSymbolCache.mFromValue.at(pAddr));
    if (mDynSymbolCache.mFromValue.contains(pAddr)) return getDynSymbol(mDynSymbolCache.mFromValue.at(pAddr));
    return std::nullopt;
}

std::optional<Symbol> ElfReader::lookupSymbol(const std::string& pName) {
    if (mSymbolCache.mFromName.contains(pName)) return getSymbol(mSymbolCache.mFromName.at(pName));
    if (mDynSymbolCache.mFromName.contains(pName)) return getDynSymbol(mDynSymbolCache.mFromName.at(pName));
    return std::nullopt;
}

std::optional<Symbol> ElfReader::getSymbol(ELFIO::section* pSec, uint64_t pIndex) {
    if (!pSec) return std::nullopt;
    Symbol                  result;
    symbol_section_accessor accessor(mImage, pSec);
    if (!accessor.get_symbol(
            pIndex,
            result.mName,
            result.mValue,
            result.mSize,
            result.mBind,
            result.mType,
            result.mSectionIndex,
            result.mOther
        )) {
        return std::nullopt;
    }
    return result;
}

std::optional<Symbol> ElfReader::getSymbol(uint64_t pIndex) { return getSymbol(_fetchSection(".symtab"), pIndex); }

std::optional<Symbol> ElfReader::getDynSymbol(uint64_t pIndex) { return getSymbol(_fetchSection(".dynsym"), pIndex); }

bool ElfReader::moveToSection(const std::string& pName) {
    auto section = _fetchSection(pName);
    return section && move(section->get_address(), Begin);
}

section* ElfReader::_fetchSection(const std::string& pSecName) const {
    for (auto& sec : mImage.sections) {
        if (sec->get_name() == pSecName) {
            return sec.get();
        }
    }
    return nullptr;
}

void ElfReader::_relocateReadonlyData() {
    // Reference:
    // https://github.com/ARM-software/abi-aa/releases/download/2023Q1/aaelf64.pdf
    // https://refspecs.linuxfoundation.org/elf/elf.pdf

    section* roData;
    if (!(roData = _fetchSection(".data.rel.ro"))) {
        return;
    }

    const auto begin      = roData->get_address();
    const auto end        = begin + roData->get_size();
    const auto EOS        = getEndOfSections();
    const auto gapInFront = getGapInFront(begin);

    forEachRelocations([&](const Relocation& pRel) {
        if (pRel.mOffset < begin || pRel.mOffset > end) {
            return;
        }
        auto offset = pRel.mOffset - gapInFront;
        auto type   = ELF64_R_TYPE(pRel.mType);
        switch (type) {
        case R_X86_64_64:
        case R_AARCH64_ABS64: {
            auto symbol = getDynSymbol(pRel.mSymbolIndex);
            if (symbol) {
                if (symbol->mValue) {
                    // Internal Symbol
                    write<Elf64_Addr, false>(offset, symbol->mValue + pRel.mAddend);
                } else {
                    // External Symbol
                    // fixme: Deviations may occur, although this does not affect data export.
                    write<Elf64_Addr, false>(offset, EOS + pRel.mSymbolIndex * sizeof(int64_t) + pRel.mAddend);
                }
            } else {
                spdlog::error("Get dynamic symbol failed!");
            }
            break;
        }
        case R_X86_64_RELATIVE:
        case R_AARCH64_RELATIVE: {
            if (!pRel.mSymbolIndex) {
                if (!pRel.mAddend) {
                    spdlog::warn("Unknown type of ADDEND detected.");
                }
                write<Elf64_Addr, false>(offset, pRel.mAddend);
            } else {
                // External
                spdlog::warn("Unhandled type of RELATIVE detected.");
            }
            break;
        }
        case R_AMDGPU_REL64:
            // TODO
        default:
            spdlog::warn("Unhandled relocation type: {}.", type);
            break;
        }
    });
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

void ElfReader::_buildSymbolCache() {

    if (!mIsValid) return;

    if (!forEachSymbols([this](uint64_t index, const Symbol& symbol) {
            mSymbolCache.mFromName.try_emplace(symbol.mName, index);
            mSymbolCache.mFromValue.try_emplace(symbol.mValue, index);
        })) {
        spdlog::warn(".symtab not found in this image!");
    }

    if (!forEachDynSymbols([this](uint64_t index, const Symbol& symbol) {
            mDynSymbolCache.mFromName.try_emplace(symbol.mName, index);
            mDynSymbolCache.mFromValue.try_emplace(getEndOfSections() + sizeof(int64_t) * index, index);
        })) {
        spdlog::warn(".dynsym not found in this image!");
    }
}
