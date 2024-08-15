//
// Created by RedbeanW on 2024/1/19.
//

#include "ItaniumVTableReader.h"

#include "format/ELF.h"
#include "format/MachO.h"

#include "util/String.h"

using JSON = nlohmann::json;

METADUMPER_ABI_ITANIUM_BEGIN

ItaniumVTableReader::ItaniumVTableReader(const std::shared_ptr<Executable>& image) : mImage(image) {
    _initFormatConstants();
    _prepareData();
}

void ItaniumVTableReader::_initFormatConstants() {
    if (dynamic_cast<format::ELF*>(mImage.get())) {
        _constant.SEGMENT_DATA          = ".data.rel.ro";
        _constant.SEGMENT_READONLY_DATA = ".rodata";
        _constant.SEGMENT_TEXT          = ".text";
        _constant.PREFIX_VTABLE         = "_ZTV";
        _constant.PREFIX_TYPEINFO       = "_ZTI";
        _constant.SYM_CLASS_INFO        = "_ZTVN10__cxxabiv117__class_type_infoE";
        _constant.SYM_SI_CLASS_INFO     = "_ZTVN10__cxxabiv120__si_class_type_infoE";
        _constant.SYM_VMI_CLASS_INFO    = "_ZTVN10__cxxabiv121__vmi_class_type_infoE";
        _constant.SYM_PURE_VFN          = "__cxa_pure_virtual";
        return;
    }
    if (dynamic_cast<format::MachO*>(mImage.get())) {
        _constant.SEGMENT_DATA          = "__const";
        _constant.SEGMENT_READONLY_DATA = "__const";
        _constant.SEGMENT_TEXT          = "__text";
        _constant.PREFIX_VTABLE         = "_ZTV"; // internal
        _constant.PREFIX_TYPEINFO       = "_ZTI";
        _constant.SYM_CLASS_INFO        = "__ZTVN10__cxxabiv117__class_type_infoE";
        _constant.SYM_SI_CLASS_INFO     = "__ZTVN10__cxxabiv120__si_class_type_infoE";
        _constant.SYM_VMI_CLASS_INFO    = "__ZTVN10__cxxabiv121__vmi_class_type_infoE";
        _constant.SYM_PURE_VFN          = "___cxa_pure_virtual";
        return;
    }
}

DumpVFTableResult ItaniumVTableReader::dumpVFTable() {
    DumpVFTableResult result;

    // Dump with symbol table:
    if (!mPrepared.mVTableBegins.empty()) {
        for (auto& addr : mPrepared.mVTableBegins) {
            mImage->move(addr, Begin);
            auto vt = readVTable();
            result.mTotal++;
            if (vt) {
                result.mVFTable.emplace_back(*vt);
                result.mParsed++;
            }
        }
        return result;
    }

    // Dump without symbol table:

    for (auto& section : mImage->getImage()->sections()) {
        if (section.name() != _constant.SEGMENT_DATA) continue;
        mImage->move(section.virtual_address(), Begin);
        while (mImage->isInSection(mImage->cur(), _constant.SEGMENT_DATA)) {
            auto backAddr = mImage->cur();
            auto expect1  = mImage->read<intptr_t>(); // offset to this
            auto expect2  = mImage->read<intptr_t>(); // type info
            auto expect3  = mImage->cur();
            auto expect4  = mImage->read<intptr_t>(); // first function
            mImage->move(backAddr, Begin);
            if (expect1 == 0 && (expect2 == 0 || mPrepared.mTypeInfoBegins.contains(expect2))
                && (mImage->isInSection(expect4, _constant.SEGMENT_TEXT)
                    || (mPrepared.mExternalSymbolPosition.contains(expect3)
                        && mPrepared.mExternalSymbolPosition.at(expect3) == _constant.SYM_PURE_VFN))) {
                auto vt = readVTable();
                if (vt) {
                    result.mVFTable.emplace_back(*vt);
                    result.mParsed++;
                }
            } else {
                mImage->move(sizeof(intptr_t));
            }
        }
    }

    return result;
}

std::string ItaniumVTableReader::_readZTS() {
    auto value = mImage->read<intptr_t>();
    // spdlog::debug("\tReading ZTS at {:#x}", value);
    if (!mImage->isInSection(value, _constant.SEGMENT_READONLY_DATA)) return {};
    auto str = mImage->readCString(value, 2048);
    return str.empty() ? str : _constant.PREFIX_TYPEINFO + str;
}

std::string ItaniumVTableReader::_readZTI() {
    auto backAddr = mImage->cur() + sizeof(intptr_t);
    auto value    = mImage->read<intptr_t>();
    if (!mImage->isInSection(value, _constant.SEGMENT_DATA)) { // external
        if (auto sym = mImage->lookupSymbol(value)) return sym->name();
        else return {};
    }
    mImage->move(value, Begin);
    mImage->move(sizeof(intptr_t)); // ignore ZTI
    auto str = _readZTS();
    mImage->move(backAddr, Begin);
    return str;
}

std::optional<VTable> ItaniumVTableReader::readVTable() {
    VTable                     result;
    std::optional<std::string> symbol;
    ptrdiff_t                  offset{};
    std::string                type;
    if (auto symbol_ = mImage->lookupSymbol(mImage->cur())) {
        symbol = symbol_->name();
        if (!symbol->starts_with(_constant.PREFIX_VTABLE)) {
            spdlog::warn("Failed to reading vtable at {:#x}. [CURRENT_IS_NOT_VTABLE]", mImage->cur());
            mImage->move(sizeof(intptr_t));
            return std::nullopt;
        }
    }
    while (true) {
        auto ptr   = mImage->cur();
        auto value = mImage->read<intptr_t>();
        // pre-check
        if (!mImage->isInSection(value, _constant.SEGMENT_TEXT)
            && !(
                mPrepared.mExternalSymbolPosition.contains(ptr)
                && mPrepared.mExternalSymbolPosition.at(ptr) == _constant.SYM_PURE_VFN
            )) {
            // read: Header
            if (value > 0) break;            // stopped.
            if (result.mSubTables.empty()) { // value == 0, is main table.
                if (value != 0) {
                    spdlog::warn(
                        "Failed to reading vtable at {:#x} in {}. [ABNORMAL_THIS_OFFSET]",
                        mImage->last(),
                        symbol.has_value() ? *symbol : "<unknown>"
                    );
                    return std::nullopt;
                }
                // read: TypeInfo
                type = _readZTI();
                if (!type.empty()) {
                    if (!type.starts_with(_constant.PREFIX_TYPEINFO)) {
                        spdlog::warn(
                            "Failed to reading vtable at {:#x} in {}. [INVALID_TYPEINFO]",
                            mImage->last(),
                            symbol.has_value() ? *symbol : "<unknown>"
                        );
                        return std::nullopt;
                    }
                    if (!symbol) symbol = _constant.PREFIX_VTABLE + StringRemovePrefix(type, _constant.PREFIX_TYPEINFO);
                    result.mTypeName = type;
                }
            } else {                   // value < 0, multi-inherited, is sub table,
                if (value == 0) break; // stopped, another vtable.
                offset = value;
                // check is same typeInfo:
                if (_readZTI() != type) {
                    spdlog::warn(
                        "Failed to reading vtable at {:#x} in {}. [TYPEINFO_MISMATCH]",
                        mImage->last(),
                        symbol.has_value() ? *symbol : "<unknown>"
                    );
                    return std::nullopt;
                }
            }
            continue;
        }
        // read: Entities
        if (mPrepared.mExternalSymbolPosition.contains(ptr)) {
            result.mSubTables[offset].emplace_back(
                VTableColumn{std::make_optional(mPrepared.mExternalSymbolPosition.at(ptr)), 0x0}
            );
        } else {
            auto curSym = mImage->lookupSymbol(value);
            result.mSubTables[offset].emplace_back(
                VTableColumn{curSym ? std::make_optional(curSym->name()) : std::nullopt, (uintptr_t)value}
            );
        }
    }
    if (!symbol) {
        spdlog::warn("Failed to reading vtable at {:#x} in <unknown>. [NAME_NOT_FOUND]", mImage->last());
        return std::nullopt;
    }
    mImage->move(-sizeof(intptr_t)); // go back.
    result.mName = *symbol;
    return result;
}

DumpTypeInfoResult ItaniumVTableReader::dumpTypeInfo() {
    DumpTypeInfoResult result;
    result.mTotal = mPrepared.mTypeInfoBegins.size();
    for (auto& addr : mPrepared.mTypeInfoBegins) {
        mImage->move(addr, Begin);
        std::unique_ptr<TypeInfo> type;
        try {
            type = readTypeInfo();
        } catch (const std::runtime_error& e) {
            spdlog::error(e.what());
            break;
        }
        if (type) {
            result.mTypeInfo.emplace_back(std::move(type));
            result.mParsed++;
        }
    }
    return result;
}

std::unique_ptr<TypeInfo> ItaniumVTableReader::readTypeInfo() {
    // Reference:
    // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#rtti-layout

    auto beginAddr = mImage->cur();

    auto inheritIndicatorValue = mImage->read<intptr_t>() - sizeof(std::type_info);

    std::string inheritIndicatorName;
    if (mPrepared.mExternalSymbolPosition.contains(beginAddr)) {
        inheritIndicatorName = mPrepared.mExternalSymbolPosition.at(beginAddr);
    } else if (auto inheritIndicator = mImage->lookupSymbol(inheritIndicatorValue)) {
        inheritIndicatorName = inheritIndicator->name();
    } else {
        spdlog::warn("Failed to reading type info at {:#x}. [CURRENT_IS_NOT_TYPEINFO]", beginAddr);
        return nullptr;
    }
    // spdlog::debug("Processing: {:#x}", beginAddr);
    if (inheritIndicatorName == _constant.SYM_CLASS_INFO) {
        auto result   = std::make_unique<NoneInheritTypeInfo>();
        result->mName = _readZTS();
        if (result->mName.empty()) {
            spdlog::warn("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", mImage->last());
            return nullptr;
        }
        return result;
    }
    if (inheritIndicatorName == _constant.SYM_SI_CLASS_INFO) {
        auto result         = std::make_unique<SingleInheritTypeInfo>();
        result->mName       = _readZTS();
        result->mOffset     = 0x0;
        result->mParentType = _readZTI();
        if (result->mName.empty() || result->mParentType.empty()) {
            spdlog::warn("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", mImage->last());
            return nullptr;
        }
        return result;
    }
    if (inheritIndicatorName == _constant.SYM_VMI_CLASS_INFO) {
        auto result   = std::make_unique<MultipleInheritTypeInfo>();
        result->mName = _readZTS();
        if (result->mName.empty()) {
            spdlog::warn("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", mImage->last());
            return nullptr;
        }
        result->mAttribute = mImage->read<unsigned int>();
        auto baseCount     = mImage->read<unsigned int>();
        for (unsigned int idx = 0; idx < baseCount; idx++) {
            BaseClassInfo baseInfo;
            baseInfo.mName = _readZTI();
            if (baseInfo.mName.empty()) {
                spdlog::warn("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", mImage->last());
                return nullptr;
            }
            auto flag        = mImage->read<long long>();
            baseInfo.mOffset = (flag >> 8) & 0xFF;
            baseInfo.mMask   = flag & 0xFF;
            result->mBaseClasses.emplace_back(baseInfo);
        }
        return result;
    }
    // spdlog::warn("Failed to reading type info at {:#x}. [UNKNOWN_INHERIT_TYPE]", beginAddr);
    return nullptr;
}

void ItaniumVTableReader::printDebugString(const VTable& pTable) {
    spdlog::info("VTable: {}", pTable.mName);
    for (auto& i : pTable.mSubTables) {
        spdlog::info("\tOffset: {:#x}", i.first);
        for (auto& j : i.second) {
            spdlog::info("\t\t{} ({:#x})", j.mSymbolName.has_value() ? "<unknown>" : *j.mSymbolName, j.mRVA);
        }
    }
}

void ItaniumVTableReader::_prepareData() {
    if (!mImage->isValid()) return;
    if (dynamic_cast<format::ELF*>(mImage.get())) {
        auto elfImage = (LIEF::ELF::Binary*)mImage->getImage();
        for (auto& symbol : elfImage->symtab_symbols()) {
            if (symbol.name().starts_with(_constant.PREFIX_VTABLE)) {
                mPrepared.mVTableBegins.emplace(symbol.value());
            } else if (symbol.name().starts_with(_constant.PREFIX_TYPEINFO)) {
                mPrepared.mTypeInfoBegins.emplace(symbol.value());
            }
        }
        for (auto& relocation : elfImage->dynamic_relocations()) {
            if (!relocation.has_symbol()) return;
            auto name = relocation.symbol()->name();
            if (name == _constant.SYM_CLASS_INFO || name == _constant.SYM_SI_CLASS_INFO
                || name == _constant.SYM_VMI_CLASS_INFO) {
                mPrepared.mTypeInfoBegins.emplace(relocation.address());
            }
        }
        return;
    }
    if (dynamic_cast<format::MachO*>(mImage.get())) {
        auto machoImage = (LIEF::MachO::Binary*)mImage->getImage();
        if (auto dyldInfo = machoImage->dyld_info()) {
            for (auto& bind : dyldInfo->bindings()) {
                if (!bind.has_symbol()) continue;
                auto name = bind.symbol()->name();
                if (name == _constant.SYM_CLASS_INFO || name == _constant.SYM_SI_CLASS_INFO
                    || name == _constant.SYM_VMI_CLASS_INFO) {
                    mPrepared.mTypeInfoBegins.emplace(bind.address());
                }
                mPrepared.mExternalSymbolPosition.try_emplace(bind.address(), name);
            }
        }
        for (auto& symbol : machoImage->symbols()) {
            if (symbol.name().starts_with(_constant.PREFIX_VTABLE)) {
                mPrepared.mVTableBegins.emplace(symbol.value());
            } else if (symbol.name().starts_with(_constant.PREFIX_TYPEINFO)) {
                mPrepared.mTypeInfoBegins.emplace(symbol.value());
            }
        }
        return;
    }
}

void ItaniumVTableReader::printDebugString(const std::unique_ptr<TypeInfo>& pType) {
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

JSON DumpVFTableResult::toJson() const {
    JSON ret;
    for (auto& i : mVFTable) {
        ret[i.mName] = i.toJson();
    }
    return ret;
}

JSON DumpTypeInfoResult::toJson() const {
    JSON ret;
    for (auto& type : mTypeInfo) {
        if (!type) continue;
        ret[type->mName] = type->toJson();
    }
    return ret;
}

METADUMPER_ABI_ITANIUM_END
