//
// Created by RedbeanW on 2023/7/27.
//

#include "Loader.h"
#include "Utils.h"

using namespace ELFIO;

Loader::~Loader() {
    mFileStream.close();
}

Loader::LoadResult Loader::load() {
    mFileStream.open(mPath, std::ios::binary);
    if (!mFileStream.is_open()) {
        return { LoadResult::OpenFileFailed };
    }
    if (!mImage.load(mFileStream)) {
        return { LoadResult::FailToParse };
    }
    mEndOfSection = _getEndOfSectionAddress();
    std::string archName;
    switch (mImage.get_machine()) {
        case EM_X86_64:
            archName = "X86_64";
            break;
        case EM_AARCH64:
            archName = "AArch64";
            break;
        default:
            return { LoadResult::UnsupportedArchitecture };
    }
    for (auto& sec : mImage.sections) {
        if (sec->get_name() == ".data.rel.ro") {
            mPreparedSections.mRELRO = sec.get();
        } else if (sec->get_name() == ".rela.dyn") {
            mPreparedSections.mRELA = sec.get();
        } else if (sec->get_name() == ".symtab") {
            mPreparedSections.mSYMTAB = sec.get();
        }
    }
    spdlog::info("Image architecture: {}.", archName);
    return { LoadResult::Ok };
}

Loader::DumpVTableResult Loader::dumpVTable(ExportVTableArguments args) {
    auto relro = mPreparedSections.mRELRO;
    if (!relro) {
        return { DumpVTableResult::SectionNotFound };
    }
    std::stringstream relroData(std::string(relro->get_data(), relro->get_size()));
    if (IsAllBytesZero(relroData, 0xF)) {
        spdlog::info("Target image needs to rebuild '.data.rel.ro', please wait...");
        relroData = _rebuildRelativeData(relro, args.mDumpSegment);
        if (!relroData.good()) {
            return { DumpVTableResult::RebuildRelativeReadOnlyDataFailed };
        }
    }
    auto relroAddr = relro->get_address();
    auto relroSize = relro->get_size();
    spdlog::info("Processing data...");
    auto rtti = _getRTTIStarts();
    auto vtable = _getVTableStarts();
    auto symCache = _buildSymbolCache();
    if (args.mVTable && (vtable.empty() || symCache.mByAddress.empty() || symCache.mByValue.empty())) {
        return { DumpVTableResult::NoSymTab };
    }
    for (auto current = relroAddr; current < relroAddr + relroSize; ) {
        if (args.mRTTI && rtti.contains(current)) {
            current += _readOneRTTI(relroData, relroAddr, current, rtti);
            continue;
        }
        if (args.mVTable && vtable.contains(current)) {
            current += _readOneVTable(relroData, relroAddr, current, rtti, symCache, vtable);
            continue;
        }
        current += 8; // unknown?
    }
    return {
        DumpVTableResult::Ok,
        rtti,
        vtable
    };
}

Loader::DumpSymbolResult Loader::dumpSymbols() {
    // todo.
    return {};
}

std::stringstream Loader::_rebuildRelativeData(ELFIO::section* relroSection, bool saveDump) {
    // Reference:
    // https://github.com/ARM-software/abi-aa/releases/download/2023Q1/aaelf64.pdf
    // https://refspecs.linuxfoundation.org/elf/elf.pdf

    auto relaSection = mPreparedSections.mRELA;
    if (!relaSection) return {};

    spdlog::info("Get section linked to RELA '{}'.", relaSection->get_name());
    const relocation_section_accessor relaTab(mImage, relaSection);
    auto relroBegin = relroSection->get_address();
    // fixme: There may be a problem with the 'size', it seems to overcalculate 0x8 bytes.
    auto relroEnd = relroSection->get_size() + relroBegin;
    auto relroData = relroSection->get_data();

    std::stringstream buffer(std::string(relroData, relroSection->get_size()));
    auto ReplaceBytes = [&](Elf64_Addr addr, uint64_t value, uint32_t length) {
        buffer.seekp((int64_t)addr);
        buffer.write(UInt64ToByteSequence(value).data(), length);
    };

    for (unsigned int i = 0; i < relaTab.get_entries_num(); ++i) {
        Elf64_Addr  offset;
        Elf_Word    symbol;
        unsigned    type;
        Elf_Sxword  addend;
        if (relaTab.get_entry(i, offset, symbol, type, addend)) {
            auto relocType = ELF64_R_TYPE(type);
            if (offset < relroBegin || offset > relroEnd) {
                // spdlog::warn("Offset out of range, ignored. (idx={}, off={:#x}, sym={}, type={}, addend={:#x})", i, offset, symbol, relocType, addend);
                continue;
            }
            auto RA = offset - relroBegin;
            switch (relocType) {
                case R_X86_64_64:
                case R_AARCH64_ABS64: {
                    symbol_section_accessor relaDynSymbols(mImage, mImage.sections[relaSection->get_link()]);
                    std::string     rName;
                    Elf64_Addr      rValue;
                    Elf_Xword       rSize;
                    unsigned char   rBind;
                    unsigned char   rType;
                    Elf_Half        rSectionIndex;
                    unsigned char   rOther;
                    relaDynSymbols.get_symbol(symbol, rName, rValue, rSize, rBind, rType, rSectionIndex, rOther);
                    if (rValue) {
                        // internal.
                        ReplaceBytes(RA, rValue + addend, 8);
                    } else {
                        // external.
                        // fixme: Deviations may occur, although, this does not affect data export.
                        ReplaceBytes(RA, mEndOfSection + (symbol - 1) * 0x8 + addend, 8);
                    }
                    break;
                }
                case R_X86_64_RELATIVE:
                case R_AARCH64_RELATIVE: {
                    if (!symbol) {
                        if (!addend) {
                            spdlog::warn("Unknown type of ADDEND detected.");
                        }
                        ReplaceBytes(RA, addend, 8);
                    } else {
                        spdlog::warn("Unhandled type of RELATIVE detected.");
                        // unhandled?
                    }
                    break;
                }
                default:
                    spdlog::warn("Unhandled relocation type: {}.", relocType);
                    break;
            }
        } else {
            spdlog::warn("Get RELA entry {} failed!", i);
        }
    }
    if (saveDump) {
        std::ofstream dumper("segment.dump", std::ios::binary);
        dumper << buffer.rdbuf();
        dumper.close();
    }
    return buffer;
}

Loader::SymbolCache Loader::_buildSymbolCache() {
    SymbolCache ret;
    if (mPreparedSections.mSYMTAB) {
        symbol_section_accessor accessor(mImage, mPreparedSections.mSYMTAB);
        for (Elf_Xword i = 0; i < accessor.get_symbols_num(); ++i) {
            std::string     name;
            Elf64_Addr      value;
            Elf_Xword       size;
            unsigned char   bind;
            unsigned char   type;
            Elf_Half        sectionIndex;
            unsigned char   other;
            accessor.get_symbol(i, name, value, size, bind, type, sectionIndex, other);
            if (!name.empty()) {
                ret.mByValue.emplace(value, name);
            }
        }
    }
    if (mPreparedSections.mRELA) {
        const relocation_section_accessor accessor(mImage, mPreparedSections.mRELA);
        for (unsigned int i = 0; i < accessor.get_entries_num(); ++i) {
            Elf64_Addr      offset;
            Elf64_Addr      symbolValue;
            std::string     symbolName;
            unsigned        type;
            Elf_Sxword      addend;
            Elf_Sxword      calcValue;
            accessor.get_entry(i, offset, symbolValue, symbolName, type, addend, calcValue);
            if (!symbolName.empty()) {
                ret.mByAddress.emplace(offset, symbolName);
            }
        }
    }
    return ret;
}

[[maybe_unused]] uint64_t Loader::_getGapInFront(ELFIO::Elf64_Addr address) {
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
        if (address >= vBegin && address < vEnd) {
            return gap;
        }
        endOfLastSegment = vEnd;
    }
    spdlog::warn("Unable to calculate gap size!");
    return 0;
}

ELFIO::Elf64_Addr Loader::_getEndOfSectionAddress() {
    ELFIO::Elf64_Addr ret = 0;
    for (auto& sec : mImage.sections) {
        auto end = sec->get_address() + sec->get_size();
        if (ret < end) {
            ret = end;
        }
    }
    return ret;
}

Loader::WholeRTTIMap Loader::_getRTTIStarts() {
    auto relaSection = mPreparedSections.mRELA;
    if (!relaSection) return {};

    const relocation_section_accessor relaTab(mImage, relaSection);
    WholeRTTIMap ret;
    for (unsigned int i = 0; i < relaTab.get_entries_num(); ++i) {
        Elf64_Addr      offset;
        Elf64_Addr      symbolValue;
        std::string     symbolName;
        unsigned        type;
        Elf_Sxword      addend;
        Elf_Sxword      calcValue;
        relaTab.get_entry(i, offset, symbolValue, symbolName, type, addend, calcValue);
        switch (H(symbolName.c_str())) {
            case H("_ZTVN10__cxxabiv117__class_type_infoE"):
                ret.try_emplace(offset, offset, None);
                break;
            case H("_ZTVN10__cxxabiv120__si_class_type_infoE"):
                ret.try_emplace(offset, offset, Single);
                break;
            case H("_ZTVN10__cxxabiv121__vmi_class_type_infoE"):
                ret.try_emplace(offset, offset, Multiple);
                break;
        }
    }

    return ret;
}

Loader::WholeVTableMap Loader::_getVTableStarts() {
    auto symSection = mPreparedSections.mSYMTAB;
    if (!symSection) return {};

    const symbol_section_accessor symTab(mImage, symSection);
    WholeVTableMap ret;
    for (unsigned int i = 0; i < symTab.get_symbols_num(); ++i) {
        std::string     name;
        Elf64_Addr      value;
        Elf_Xword       size;
        unsigned char   bind;
        unsigned char   type;
        Elf_Half        sectionIndex;
        unsigned char   other;
        symTab.get_symbol(i, name, value, size, bind, type, sectionIndex, other);
        if (name.starts_with("_ZTV")) {
            ret.try_emplace(value, name);
        }
    }
    return ret;
}

uint64_t Loader::_readOneRTTI(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr starts, WholeRTTIMap& allRttiInfo) {
    // 'attribute' reference:
    // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#rtti-layout

    if (!allRttiInfo.contains(starts)) {
        return 0;
    }
    auto& ins = allRttiInfo.at(starts);
    unsigned char buffer[8]; // tmp
    ins.mName = _readOneTypeName(relroData, relroBase, starts);
    switch (ins.mInherit) {
        case None:
            // done.
            break;
        case Single: {
            relroData.read((char*)buffer, 8);
            BaseClassInfo base;
            auto strAddr = FromBytes<uint64_t>(buffer);
            if (strAddr >= mEndOfSection) {
                ins.mWeak = true;
                break;
            }
            base.mName = _readOneTypeName(relroData, relroBase, strAddr, true);
            base.mOffset = 0x0;
            ins.mParents.emplace(base);
            break;
        }
        case Multiple: {
            relroData.read((char*)buffer, 4);
            ins.mAttribute = FromBytes<Flag>(buffer);
            relroData.read((char*)buffer, 4); // read base class count.
            auto baseClassCount = FromBytes<int32_t>(buffer);
            for (int32_t i = 0; i < baseClassCount; i++) {
                relroData.read((char*)buffer, 8); // read base class' type name.
                BaseClassInfo base;
                auto strAddr = FromBytes<uint64_t>(buffer);
                if (strAddr >= mEndOfSection) {
                    ins.mWeak = true;
                    break;
                }
                base.mName = _readOneTypeName(relroData, relroBase, strAddr, true);
                relroData.read((char*)buffer, 8); // read base class' attributes
                auto attribute = FromBytes<Flag>(buffer);
                base.mOffset = (attribute >> 8) & 0xFF;
                base.mMask = attribute & 0xFF;
                ins.mParents.emplace(base);
            }
            break;
        }
        default:
            spdlog::warn("Unknown inherit type detected when reading rtti information!");
            break;
    }
    return (uint64_t)relroData.tellg() - (starts - relroBase);
}

uint64_t Loader::_readOneVTable(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr starts, WholeRTTIMap& allRttiInfo, const SymbolCache& symCache, WholeVTableMap& allVTableInfo) {
    //if (starts == 0x742DA60) {
    //    __debugbreak();
    //}
    if (!allVTableInfo.contains(starts)) {
        return 0;
    }
    relroData.clear();
    relroData.seekg((int64_t)(starts - relroBase));
    unsigned char buffer[8]; // tmp
    std::vector<Elf64_Addr> created; // used to rollback.
    created.emplace_back(starts);
    uint32_t idx = 0;
    auto getSymbol = [&](uint64_t value, Elf64_Addr addr) -> std::string {
        if (symCache.mByAddress.contains(addr)) {
            return symCache.mByAddress.at(addr);
        }
        if (symCache.mByValue.contains(value)) {
            return symCache.mByValue.at(value);
        }
        return {};
    };
    bool offsetValid = false;
    while (true) {
        auto RA = relroBase + relroData.tellg();
        if (idx && (allVTableInfo.contains(RA) || allRttiInfo.contains(RA))) {
            break; // completed.
        }
        idx++;
        relroData.read((char*)buffer, 8);
        auto value = FromBytes<int64_t>(buffer);
        if (value <= 0) { // offset to this
            if (offsetValid && value >= allVTableInfo.at(starts).mOffset) {
                relroData.seekg(-8, std::ios::cur);
                // fixme: Align what???
                // spdlog::warn("unknown align at {:#x} ignored.", RA);
                break;
            }
            relroData.read((char*)buffer, 8);
            auto addr = FromBytes<int64_t>(buffer);
            auto sym = getSymbol(addr, RA);
            if (sym.empty() || !sym.starts_with("_ZTI")) break;
            if (value != 0) {
                allVTableInfo.try_emplace(RA, sym);
                starts = RA;
                created.emplace_back(RA);
            } else {
                allVTableInfo.at(starts).mName = sym;
            }
            allVTableInfo.at(starts).mOffset = value;
            offsetValid = true;
            continue;
        }
        auto sym = getSymbol(value, RA);
        if (!sym.empty()) {
            if (sym.starts_with("_ZTV")) break; // unfiltered.
            allVTableInfo.at(starts).mEntries.emplace(idx, value, sym);
        } else {
            spdlog::warn("Fail to parse vtable: {}({:#x}).", allVTableInfo.at(starts).mName, RA);
            for (auto& i : created) { // erase and rollback
                allVTableInfo.erase(i);
            }
            return 0;
        }
    }
    return relroBase + (uint64_t)relroData.tellg() - starts;
}

void Loader::_readCString(int64_t begin, std::string& result) {
    char buffer[2048];
    mFileStream.clear();
    mFileStream.seekg(begin, std::ios::beg);
    mFileStream.read(buffer, 2048);
    result.clear();
    for (char chr : buffer) {
        if (chr == '\0') {
            break;
        }
        result += chr;
    }
}

std::string Loader::_readOneTypeName(std::stringstream &relroData, uint64_t relroBase, ELFIO::Elf64_Addr begin, bool keepPtrPos) {
    std::streampos cur;
    if (keepPtrPos) cur = relroData.tellg();
    relroData.clear();
    relroData.seekg((int64_t)(begin + 8 - relroBase)); // ignore top pointer.
    unsigned char buffer[8]; // tmp
    relroData.read((char*)buffer, 8); // read type name
    std::string ret;
    _readCString((int64_t) FromBytes<uint64_t>(buffer), ret);
    if (keepPtrPos) relroData.seekg(cur);
    return ret;
}

std::string Loader::LoadResult::getErrorMsg() const {
    switch (mResult) {
        case Ok:
            return "Image loaded successfully.";
        case OpenFileFailed:
            return "Failed to open the file, please check if the file exists and can be read.";
        case FailToParse:
            return "Could not read file as a valid ELF file.";
        case UnsupportedArchitecture:
            return "Sorry, the architecture of this file is not supported at this time.";
        default:
            return "An unknown error has occurred.";
    }
}

std::string Loader::DumpVTableResult::getErrorMsg() const {
    switch (mResult) {
        case Ok:
            return "The vtable is successfully dumped.";
        case SectionNotFound:
            return "Could not find necessary segment in ELF file (.data.rel.ro/.dynsym).";
        case RebuildRelativeReadOnlyDataFailed:
            return "Rebuilding relative read-only data failed!";
        case NoSymTab:
            return "Target image lacks symbol information needed to export vtables.";
        default:
            return "An unknown error has occurred.";
    }
}
