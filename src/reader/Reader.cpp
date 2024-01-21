//
// Created by RedbeanW on 2024/1/19.
//

#include "Reader.h"

#include <fstream>

using namespace ELFIO;

Reader::Reader(const std::string& pPath) {
    std::ifstream file;
    file.open(pPath, std::ios::binary);
    if (!file.is_open()) {
        mState.setError("Failed to open file.");
        return;
    }
    if (!mImage.load(file)) {
        mState.setError("Failed to load elf image.");
        return;
    }
    mStream << file.rdbuf();
    file.close();
    _relocateReadonlyData();
}

ReaderState Reader::getState() const {
    return mState;
}

Elf64_Addr Reader::getEndOfSections() const {
    Elf64_Addr ret = 0;
    for (auto& sec : mImage.sections) {
        auto end = sec->get_address() + sec->get_size();
        if (ret < end) {
            ret = end;
        }
    }
    READER_UNREACHABLE
    return 0;
}

Architecture Reader::getArchitecture() const {
    switch (mImage.get_machine()) {
        case EM_X86_64:
            return Architecture::X86_64;
        case EM_AARCH64:
            return Architecture::AArch64;
        default:
            return Architecture::Unsupported;
    }
}

uint64_t Reader::getGapInFront(Elf64_Addr pAddr) const {
    const uint32_t LOAD = 1;
    uint64_t gap = 0;
    uint64_t endOfLastSegment = 0;
    for (auto& seg : mImage.segments) {
        if (seg->get_type() != LOAD) {
            continue;
        }
        auto vBegin = seg->get_virtual_address();
        auto vEnd = vBegin + seg->get_file_size();
        gap += vBegin - endOfLastSegment;
        if (pAddr >= vBegin && pAddr < vEnd) {
            return gap;
        }
        endOfLastSegment = vEnd;
    }
    READER_UNREACHABLE
    return -1;
}

std::string Reader::readCString(size_t pMaxLength) {
    std::string result;
    for (size_t i = 0; i < pMaxLength; i++) {
        auto chr = read<char>();
        if (chr == '\0') {
            break;
        }
        result += chr;
    }
    return result;
}

std::string Reader::readCString(Elf64_Addr pAddr, size_t pMaxLength) {
    auto after = cur();
    reset();
    move(pAddr, Begin);
    auto result = readCString(pMaxLength);
    move(after, Begin);
    return result;
}

bool Reader::forEachSymbols(ELFIO::section* pSec, const std::function<void(Symbol)>& pCall) {
    if (!pSec) return false;
    symbol_section_accessor accessor(mImage, pSec);
    if (accessor.get_symbols_num() <= 0) return false;
    for (Elf_Xword idx = 0; idx < accessor.get_symbols_num(); ++idx) {
        std::string     name;
        Elf64_Addr      value;
        Elf_Xword       size;
        unsigned char   bind;
        unsigned char   type;
        Elf_Half        sectionIndex;
        unsigned char   other;
        accessor.get_symbol(idx, name, value, size, bind, type, sectionIndex, other);
        pCall(Symbol {name, value, size, bind, type, sectionIndex, other});
    }
    return true;
}

bool Reader::forEachSymbols(const std::function<void(Symbol)>& pCall) {
    return forEachSymbols(_fetchSection(".symtab"), pCall);
}

bool Reader::forEachRelocations(const std::function<void(Relocation)>& pCall) {
    // todo: relaPlt
    section* relaDyn;
    if (!(relaDyn = _fetchSection(".rela.dyn"))) {
        return false;
    }
    const relocation_section_accessor accessor(mImage, relaDyn);
    for (Elf_Xword idx = 0; idx < accessor.get_entries_num(); ++idx) {
        Elf64_Addr      offset;
        Elf64_Addr      symbolValue;
        std::string     symbolName;
        unsigned        type;
        Elf_Sxword      addend;
        Elf_Sxword      calcValue;
        accessor.get_entry(idx, offset, symbolValue, symbolName, type, addend, calcValue);
        pCall(Relocation {offset, symbolValue, symbolName, type, addend, calcValue});
    }
    return true;
}

std::optional<Symbol> Reader::lookupSymbol(Elf64_Addr pAddr) {
    Symbol result;
    section* symTab;
    if (!(symTab = _fetchSection(".symtab"))) {
        return std::nullopt;
    }
    symbol_section_accessor accessor(mImage, symTab);
    if (!accessor.get_symbol(pAddr, result.mName, result.mSize, result.mBind, result.mType, result.mSectionIndex, result.mOther)) {
        return std::nullopt;
    }
    result.mValue = pAddr;
    return result;
}

std::optional<Symbol> Reader::lookupSymbol(const std::string& pName) {
    Symbol result;
    section* symTab;
    if (!(symTab = _fetchSection(".symtab"))) {
        return std::nullopt;
    }
    symbol_section_accessor accessor(mImage, symTab);
    if (!accessor.get_symbol(pName, result.mValue, result.mSize, result.mBind, result.mType, result.mSectionIndex, result.mOther)) {
        return std::nullopt;
    }
    result.mName = pName;
    return result;
}

section* Reader::_fetchSection(const char* pSecName) {
    for (auto& sec : mImage.sections) {
        if (sec->get_name() == pSecName) {
            return sec.get();
        }
    }
    return nullptr;
}

void Reader::_relocateReadonlyData() {
    // Reference:
    // https://github.com/ARM-software/abi-aa/releases/download/2023Q1/aaelf64.pdf
    // https://refspecs.linuxfoundation.org/elf/elf.pdf

    section* roData;
    if (!(roData = _fetchSection(".data.rel.ro"))) {
        return;
    }

    // fixme: There may be a problem with the 'size', it seems to over-calculate 0x8 bytes.
    auto begin = roData->get_address();
    auto end = begin + roData->get_size();

    forEachRelocations([&](const Relocation& pRel) {
        auto type = ELF64_R_TYPE(pRel.mType);
        if (pRel.mOffset < begin || pRel.mOffset > end) {
            // spdlog::warn("Offset out of range, ignored. (idx={}, off={:#x}, sym={}, type={}, addend={:#x})", i, offset, symbol, type, addend);
            return;
        }
        switch (type) {
            case R_X86_64_64:
            case R_AARCH64_ABS64: {
                spdlog::debug("hi {}", mImage.sections[_fetchSection(".rela.dyn")->get_link()]->get_name());
                forEachSymbols(mImage.sections[_fetchSection(".rela.dyn")->get_link()], [&](const Symbol &pSym) {
                    if (pSym.mValue) {
                        // internal,
                        write<Elf64_Addr, false>(pRel.mOffset, pSym.mValue + pRel.mAddend);
                    } else {
                        // external,
                        // fixme: Deviations may occur, although, this does not affect data export.
                        // ReplaceBytes(RA, mEndOfSection + (symbol - 1) * 0x8 + addend, 8);
                    }
                });
            }
            case R_X86_64_RELATIVE:
            case R_AARCH64_RELATIVE: {
                if (pRel.mSymbolName.empty()) {
                    if (!pRel.mAddend) {
                        spdlog::warn("Unknown type of ADDEND detected.");
                    }
                    write<Elf64_Addr, false>(pRel.mOffset, pRel.mAddend);
                } else {
                    // external,
                    // spdlog::warn("Unhandled type of RELATIVE detected.");
                }
                break;
            }
            default:
                spdlog::warn("Unhandled relocation type: {}.", type);
                break;
        }
    });

#ifdef DUMP_SEGMENT
    std::ofstream dumper("segment.dump", std::ios::binary);
    dumper << buffer.rdbuf();
    dumper.close();
#endif
}
