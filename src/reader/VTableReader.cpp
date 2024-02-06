//
// Created by RedbeanW on 2024/1/19.
//

#include "VTableReader.h"

VTableReader::VTableReader(const std::string& pPath) : ElfReader(pPath) {
    _prepareData();
}

DumpVTableResult VTableReader::dumpVTable() {
#if 1
    DumpVTableResult result;
    for (auto& addr : mPrepared.mVTableBegins) {
        move(addr, Begin);
        auto vt = readVTable();
        if (vt) {
            result.mVFTable.emplace_back(*vt);
        } else {
            result.mUnparsedVFTableCount++;
        }
    }
    for (auto& addr : mPrepared.mTypeInfoBegins) {
        move(addr, Begin);
        auto type = readTypeInfo();
        if (type) {
            result.mTypeInfo.emplace_back(std::move(type));
        } else {
            result.mUnparsedTypeInfoCount++;
        }
    }
    return result;
#else
    move(0x0000000007EFAC40, Begin);
    auto vtable = readVTable();
    if (vtable) {
        printDebugString(*vtable);
    }
    return {};
#endif
}

std::optional<VTable> VTableReader::readVTable() {
    VTable result;
    auto vtSym = lookupSymbol(cur());
    if (!vtSym || !vtSym->mName.starts_with("_ZTV")) {
        spdlog::error("Failed to reading vtable at {:#x}. [CURRENT_IS_NOT_VTABLE]", cur());
        return std::nullopt;
    }
    result.mName = vtSym->mName;
    ptrdiff_t offsetToThis {0};
    Elf64_Addr typeInfoPtr;
    while (true) {
        auto value = read<intptr_t>();
        // pre-check
        if (mPrepared.mTypeInfoBegins.contains(last())) {
            break; // stopped, another typeinfo.
        }
        if (mPrepared.mLambdaBegins.contains(last())) {
            break; // stopped, found _ZZZZN.
        }
        if (isInSection(value, ".rodata")) {
            break; // stopped, another data.
        }
        if (isInSection(value, ".data.rel.ro")) {
            break; // stopped, static member pointer.
        }
        // read: Header
        if (value <= 0) {
            if (result.mSubTables.empty()) { // value == 0 (main table).
                if (value != 0) {
                    spdlog::error("Failed to reading vtable at {:#x} in {}. [ABNORMAL_THIS_OFFSET]", last(), vtSym->mName);
                    return std::nullopt;
                }
                // read: TypeInfo
                typeInfoPtr = read<Elf64_Addr>(); // todo: Add support for rtti-less image.
                auto typeInfoSym = lookupSymbol(typeInfoPtr);
                if (!typeInfoSym || !typeInfoSym->mName.starts_with("_ZTI")) {
                    spdlog::error("Failed to reading vtable at {:#x} in {}. [ABNORMAL_TYPEINFO_PTR]", last(), vtSym->mName);
                    return std::nullopt;
                }
                result.mTypeName = typeInfoSym->mName;
            } else {
                if (value == 0) {
                    break; // stopped, another vtable.
                }
                offsetToThis = value; // value < 0 (sub table).
                // check: TypeInfo
                if (read<Elf64_Addr>() != typeInfoPtr) {
                    spdlog::error("Failed to reading vtable at {:#x} in {}. [TYPEINFO_PTR_MISMATCH]", last(), vtSym->mName);
                    return std::nullopt;
                }
            }
            continue;
        }
        // read: Entities
        auto curSym = lookupSymbol(value);
        if (!curSym) {
            spdlog::error("Failed to reading vtable at {:#x} in {}. [UNHANDLED_POSITION]", last(), vtSym->mName);
            return std::nullopt;
        }
        result.mSubTables[offsetToThis].emplace_back(VTableColumn {curSym->mName, curSym->mValue});
    }
    return result;
}

std::unique_ptr<TypeInfo> VTableReader::readTypeInfo() {
    // Reference:
    // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#rtti-layout

    auto typeSym = lookupSymbol(cur());
    if (!typeSym || !typeSym->mName.starts_with("_ZTI")) {
        spdlog::error("Failed to reading type info at {:#x}. [CURRENT_IS_NOT_TYPEINFO]", cur());
        return nullptr;
    }

    auto value = read<int64_t>();
    //auto EOS = getEndOfSections();
    value -= 0x10; // fixme: Why need to do this?

    auto inheritIndicator = lookupSymbol(value);
    if (!inheritIndicator) return nullptr;
    switch (H(inheritIndicator->mName.c_str())) {
    case H("_ZTVN10__cxxabiv117__class_type_infoE"): {
        auto result = std::make_unique<NoneInheritTypeInfo>();
        result->mName = typeSym->mName;
        return result;
    }
    case H("_ZTVN10__cxxabiv120__si_class_type_infoE"): {
        auto result = std::make_unique<SingleInheritTypeInfo>();
        move(8); // jump out _ZTS...
        auto baseTypeSym = lookupSymbol(read<int64_t>());
        if (!baseTypeSym) {
            spdlog::error("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", last());
            return nullptr;
        }
        result->mName = typeSym->mName;
        result->mOffset = 0x0;
        result->mParentType = baseTypeSym->mName;
        return result;
    }
    case H("_ZTVN10__cxxabiv121__vmi_class_type_infoE"): {
        auto result = std::make_unique<MultipleInheritTypeInfo>();
        move(8); // jump out _ZTS...
        result->mName = typeSym->mName;
        result->mAttribute = read<unsigned int>();
        auto baseCount = read<unsigned int>();
        for (unsigned int idx = 0; idx < baseCount; idx++) {
            BaseClassInfo baseInfo;
            auto baseTypeSym = lookupSymbol(read<int64_t>());
            if (!baseTypeSym) {
                spdlog::error("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", last());
                return nullptr;
            }
            auto flag = read<long long>();
            baseInfo.mName   = baseTypeSym->mName;
            baseInfo.mOffset = (flag >> 8) & 0xFF;
            baseInfo.mMask   = flag & 0xFF;
            result->mBaseClasses.emplace_back(baseInfo);
        }
        return result;
    }
    }
    // spdlog::error("Unhandled inherit type at {:#x}", last());
    return nullptr;
}

void VTableReader::printDebugString(const VTable& pTable) {
    spdlog::info("VTable: {}", pTable.mName);
    for (auto& i : pTable.mSubTables) {
        spdlog::info("\tOffset: {:#x}", i.first);
        for (auto& j : i.second) {
            spdlog::info("\t\t{} ({:#x})", j.mSymbolName, j.mRVA);
        }
    }
}

void VTableReader::_prepareData() {

    if (!mIsValid) return;

    auto result = forEachSymbols([this](uint64_t pIndex, const Symbol& pSym) {
        if (pSym.mName.starts_with("_ZTV")) {
            mPrepared.mVTableBegins.emplace(pSym.mValue);
        } else if (pSym.mName.starts_with("_ZTI")) {
            mPrepared.mTypeInfoBegins.emplace(pSym.mValue);
        } else if (pSym.mName.starts_with("_ZZZZN")) {
            mPrepared.mLambdaBegins.emplace(pSym.mValue);
        }
    });

    if (!result) {
        spdlog::error("No symbols found in this image!");
        mIsValid = false;
    } else {
        spdlog::info("Found {} vtables and {} typeinfo in this image.", mPrepared.mVTableBegins.size(), mPrepared.mTypeInfoBegins.size());
    }

}
void VTableReader::printDebugString(const std::unique_ptr<TypeInfo>& pType) {
    if (!pType) return;
    spdlog::info("TypeInfo: {}", pType->mName);
    switch (pType->kind()) {
    case TypeInheritKind::None:
        spdlog::info("\tInherit: None");
        break;
    case TypeInheritKind::Single: {
        spdlog::info("\tInherit: Single");
        auto typeInfo = (SingleInheritTypeInfo*)pType.get();
        spdlog::info("\tParentType: {}", typeInfo->mParentType);
        spdlog::info("\tOffset: {:#x}", typeInfo->mOffset);
        break;
    }
    case TypeInheritKind::Multiple: {
        spdlog::info("\tInherit: Multiple");
        auto typeInfo = (MultipleInheritTypeInfo*)pType.get();
        spdlog::info("\tAttribute: {:#x}", typeInfo->mAttribute);
        spdlog::info("\tBase classes ({}):", typeInfo->mBaseClasses.size());
        for (auto& base : typeInfo->mBaseClasses) {
            spdlog::info("\t\tOffset: {:#x}", base.mOffset);
            spdlog::info("\t\t\tName: {}", base.mName);
            spdlog::info("\t\t\tMask: {:#x}", base.mMask);
        }
        break;
    }
    }
}
