//
// Created by RedbeanW on 2023/7/27.
//

#include "Loader.h"
#include "Utils.h"

Loader::~Loader() {
    mFileStream.close();
}

Loader::LoadResult Loader::load() {
    using namespace ELFIO;
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
    spdlog::info("Image architecture: {}.", archName);
    return { LoadResult::Ok };
}

Loader::DumpVTableResult Loader::dumpVTable(ExportVTableArguments args) {
    auto relro = _getSection(".data.rel.ro");
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
    auto rtti = args.mRTTI ? _getRTTIStarts() : (WholeRTTIMap {});
    for (auto current = relroAddr; current < relroAddr + relroSize; ) {
        if (args.mRTTI) {
            if (rtti.contains(current)) {
                current += _readOneRTTI(relroData, relroAddr, current, rtti);
                continue;
            }
        }
        if (args.mVTable) {
            if (IsAllBytesZero(relroData, 8, (int64_t)(current - relroAddr))) {
                // todo.
                current += 8;
                continue;
            }
        }
        current += 8; // default.
    }
    return {
        DumpVTableResult::Ok,
        std::move(rtti)
    };
}

Loader::DumpSymbolResult Loader::dumpSymbols() {
    // todo.
    return {};
}

std::stringstream Loader::_rebuildRelativeData(ELFIO::section* relroSection, bool saveDump) {
#define ReplaceBytes(value, length)         buffer.seekp(RA); \
                                            buffer.write(UInt64ToByteSequence((value)).data(), (length))
    // ELF for the AArch64 Reference:
    // https://github.com/ARM-software/abi-aa/releases/download/2023Q1/aaelf64.pdf

    using namespace ELFIO;
    auto relaSection = _getSection(SHT_RELA);
    if (!relaSection) return {};

    spdlog::info("Get section linked to RELA '{}'.", relaSection->get_name());
    const relocation_section_accessor relaTab(mImage, relaSection);
    auto relroBegin = relroSection->get_address();
    // fixme: There may be a problem with the 'size', it seems to overcalculate 0x8 bytes.
    auto relroEnd = relroSection->get_size() + relroBegin;
    auto relroData = relroSection->get_data();

    std::stringstream buffer(std::string(relroData, relroSection->get_size()));

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
                        ReplaceBytes(rValue + addend, 8);
                    } else {
                        // external.
                        // fixme: Deviations may occur, although, this does not affect data export.
                        ReplaceBytes(mEndOfSection + (symbol - 1) * 0x8 + addend, 8);
                    }
                    break;
                }
                case R_AARCH64_RELATIVE: {
                    if (!symbol) {
                        if (!addend) {
                            spdlog::warn("Unknown type of ADDEND detected.");
                        }
                        ReplaceBytes(addend, 8);
                    } else {
                        spdlog::warn("Unhandled type of RELATIVE detected.");
                        // unhandled?
                    }
                    break;
                }
                case R_AARCH64_GLOB_DAT:
                case R_AARCH64_COPY:
                case R_AARCH64_JUMP_SLOT:
                case R_AARCH64_TLS_IMPDEF1:
                case R_AARCH64_TLS_IMPDEF2:
                case R_AARCH64_TLS_TPREL:
                case R_AARCH64_TLSDESC:
                case R_AARCH64_IRELATIVE:
                    spdlog::warn("Unhandled relocation type: {}.", relocType);
                    break;
                default:
                    spdlog::warn("Unrecognized relocation type: {}.", relocType);
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

ELFIO::section* Loader::_getSection(const std::string& name) {
    for (auto& sec : mImage.sections) {
        if (sec->get_name() == name) {
            return sec.get();
        }
    }
    return nullptr;
}

ELFIO::section *Loader::_getSection(ELFIO::Elf_Word type) {
    for (auto& sec : mImage.sections) {
        if (sec->get_type() == type) {
            return sec.get();
        }
    }
    return nullptr;
}

Loader::WholeRTTIMap Loader::_getRTTIStarts() {
    using namespace ELFIO;
    auto relaSection = _getSection(SHT_RELA);
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

uint64_t Loader::_readOneRTTI(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr starts, WholeRTTIMap& allRttiInfo) {
    if (!allRttiInfo.contains(starts)) {
        return 0;
    }
    auto& ins = allRttiInfo.at(starts);
    unsigned char buffer[8]; // tmp
    auto readOneTypeName = [&](ELFIO::Elf64_Addr begin, bool keepPtrPos = false) -> std::string {
        std::streampos cur;
        if (keepPtrPos) cur = relroData.tellg();
        relroData.clear();
        relroData.seekg((int64_t)(begin - relroBase + 8)); // ignore top pointer.
        relroData.read((char*)buffer, 8); // read type name
        std::string ret;
        _readCString((int64_t) FromBytes<uint64_t>(buffer), ret);
        if (keepPtrPos) relroData.seekg(cur);
        return ret;
    };
    ins.mName = readOneTypeName(starts);
    // 'attribute' reference:
    // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#rtti-layout
    switch (ins.mInherit) {
        case None:
            // done.
            break;
        case Single: {
            relroData.read((char*)buffer, 8);
            auto base = std::make_unique<BaseClassInfo>();
            auto strAddr = FromBytes<uint64_t>(buffer);
            if (strAddr >= mEndOfSection) {
                ins.mWeak = true;
                break;
            }
            base->mName = readOneTypeName(strAddr, true);
            base->mOffset = 0x0;
            ins.mParents.emplace(std::move(base));
            break;
        }
        case Multiple: {
            relroData.read((char*)buffer, 4);
            ins.mAttribute = FromBytes<Flag>(buffer);
            relroData.read((char*)buffer, 4); // read base class count.
            auto baseClassCount = FromBytes<int32_t>(buffer);
            for (int32_t i = 0; i < baseClassCount; i++) {
                relroData.read((char*)buffer, 8); // read base class' type name.
                auto base = std::make_unique<BaseClassInfo>();
                auto strAddr = FromBytes<uint64_t>(buffer);
                if (strAddr >= mEndOfSection) {
                    ins.mWeak = true;
                    break;
                }
                base->mName = readOneTypeName(strAddr, true);
                relroData.read((char*)buffer, 8); // read base class' attributes
                auto attribute = FromBytes<Flag>(buffer);
                base->mOffset = (attribute >> 8) & 0xFF;
                base->mMask = attribute & 0xFF;
                ins.mParents.emplace(std::move(base));
            }
            break;
        }
        default:
            spdlog::warn("Unknown inherit type detected when reading rtti information!");
            break;
    }
    return (uint64_t)relroData.tellg() - (starts - relroBase);
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
        default:
            return "An unknown error has occurred.";
    }
}
