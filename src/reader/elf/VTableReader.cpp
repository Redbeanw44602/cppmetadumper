//
// Created by RedbeanW on 2024/1/19.
//

#include "VTableReader.h"

#include "util/Hash.h"

using JSON = nlohmann::json;

METADUMPER_ELF_BEGIN

JSON VTable::toJson() const {
    auto subTables = JSON::array();
    for (auto& j : mSubTables) {
        auto entities = JSON::array();
        for (auto& k : j.second) {
            entities.emplace_back(k.toJson());
        }
        subTables.emplace_back(JSON{
            {"offset",   j.first },
            {"entities", entities}
        });
    }
    return JSON{
        {"sub_tables", subTables                                   },
        {"type_name",  mTypeName.has_value() ? *mTypeName : nullptr}
    };
}

JSON VTableColumn::toJson() const {
    return JSON{
        {"symbol", mSymbolName.has_value() ? JSON(*mSymbolName) : JSON{}},
        {"rva",    mRVA                                                 }
    };
}

JSON NoneInheritTypeInfo::toJson() const {
    return JSON{
        {"inherit_type", "None"}
    };
}

JSON SingleInheritTypeInfo::toJson() const {
    return JSON{
        {"inherit_type", "Single"   },
        {"parent_type",  mParentType},
        {"offset",       mOffset    }
    };
}

JSON MultipleInheritTypeInfo::toJson() const {
    auto baseClasses = JSON::array();
    for (auto& base : mBaseClasses) {
        baseClasses.emplace_back(JSON{
            {"offset", base.mOffset},
            {"name",   base.mName  },
            {"mask",   base.mMask  }
        });
    }
    return JSON{
        {"inherit_type", "Multiple" },
        {"attribute",    mAttribute },
        {"base_classes", baseClasses}
    };
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

DumpVFTableResult VTableReader::dumpVFTable() {
    DumpVFTableResult result;

    // Dump with symbol table:
    if (!mPrepared.mVTableBegins.empty()) {
        for (auto& addr : mPrepared.mVTableBegins) {
            move(addr, Begin);
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
    if (!moveToSection(".data.rel.ro")) {
        spdlog::error("Unable to move to target section.");
        return result;
    }

    while (isInSection(cur(), ".data.rel.ro")) {
        auto backAddr = cur();
        auto expect1  = read<intptr_t>();
        auto expect2  = read<intptr_t>();
        auto expect3  = read<intptr_t>();
        move(backAddr, Begin);
        if (expect1 == 0 && (expect2 == 0 || mPrepared.mTypeInfoBegins.contains(expect2))
            && isInSection(expect3, ".text")) {
            auto vt = readVTable();
            if (vt) {
                result.mVFTable.emplace_back(*vt);
                result.mParsed++;
            }
        } else {
            move(sizeof(intptr_t));
        }
    }

    return result;
}

std::string VTableReader::_readZTS() {
    auto value = read<intptr_t>();
    // spdlog::debug("\tReading ZTS at {:#x}", value);
    auto str = readCString(value, 2048);
    return str.empty() ? str : "_ZTI" + str;
}

std::string VTableReader::_readZTI() {
    auto backAddr = cur() + sizeof(intptr_t);
    auto value    = read<intptr_t>();
    if (!isInSection(value, ".data.rel.ro")) { // external
        if (auto sym = lookupSymbol(value)) return sym->name();
        else return {};
    }
    move(value, Begin);
    move(sizeof(intptr_t)); // ignore ZTI
    auto str = _readZTS();
    move(backAddr, Begin);
    return str;
}

std::optional<VTable> VTableReader::readVTable() {
    VTable                     result;
    std::optional<std::string> symbol;
    ptrdiff_t                  offset{};
    std::string                type;
    if (auto symbol_ = lookupSymbol(cur())) {
        symbol = symbol_->name();
        if (!symbol->starts_with("_ZTV")) {
            spdlog::error("Failed to reading vtable at {:#x}. [CURRENT_IS_NOT_VTABLE]", cur());
            return std::nullopt;
        }
    }
    while (true) {
        auto value = read<intptr_t>();
        // pre-check
        if (!isInSection(value, ".text")) {
            // read: Header
            if (value > 0) break;            // stopped.
            if (result.mSubTables.empty()) { // value == 0, is main table.
                if (value != 0) {
                    spdlog::error(
                        "Failed to reading vtable at {:#x} in {}. [ABNORMAL_THIS_OFFSET]",
                        last(),
                        symbol.has_value() ? *symbol : "<unknown>"
                    );
                    return std::nullopt;
                }
                // read: TypeInfo
                type = _readZTI();
                if (!type.empty()) {
                    if (!symbol) symbol = "_ZTV" + type.substr(4); // "_ZTI".length == 4
                    result.mTypeName = type;
                }
            } else {                   // value < 0, multi-inherited, is sub table,
                if (value == 0) break; // stopped, another vtable.
                offset = value;
                // check is same typeInfo:
                if (_readZTI() != type) {
                    spdlog::error(
                        "Failed to reading vtable at {:#x} in {}. [TYPEINFO_MISMATCH]",
                        last(),
                        symbol.has_value() ? "<unknown>" : *symbol
                    );
                    return std::nullopt;
                }
            }
            continue;
        }
        // read: Entities
        auto curSym = lookupSymbol(value);
        result.mSubTables[offset].emplace_back(
            VTableColumn{curSym ? std::make_optional(curSym->name()) : std::nullopt, (uintptr_t)value}
        );
    }
    if (!symbol) {
        spdlog::warn("Failed to reading vtable at {:#x} in <unknown>. [NAME_NOT_FOUND]", last());
        return std::nullopt;
    }
    result.mName = *symbol;
    return result;
}

DumpTypeInfoResult VTableReader::dumpTypeInfo() {
    DumpTypeInfoResult result;
    result.mTotal = mPrepared.mTypeInfoBegins.size();
    for (auto& addr : mPrepared.mTypeInfoBegins) {
        move(addr, Begin);
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

std::unique_ptr<TypeInfo> VTableReader::readTypeInfo() {
    // Reference:
    // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#rtti-layout

    auto beginAddr = cur();

    if (beginAddr == 0xffffffffffffffff) {
        // Bad image.
        throw std::runtime_error("For some unknown reason, the reading process stopped.");
    }

    auto inheritIndicatorValue = read<intptr_t>() - 0x10; // see std::type_info

    auto inheritIndicator = lookupSymbol(inheritIndicatorValue);
    if (!inheritIndicator) {
        spdlog::error("Failed to reading type info at {:#x}. [CURRENT_IS_NOT_TYPEINFO]", beginAddr);
        return nullptr;
    }
    // spdlog::debug("Processing: {:#x}", beginAddr);
    switch (H(inheritIndicator->name().c_str())) {
    case H("_ZTVN10__cxxabiv117__class_type_infoE"): {
        auto result   = std::make_unique<NoneInheritTypeInfo>();
        result->mName = _readZTS();
        if (result->mName.empty()) {
            spdlog::error("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", last());
            return nullptr;
        }
        return result;
    }
    case H("_ZTVN10__cxxabiv120__si_class_type_infoE"): {
        auto result         = std::make_unique<SingleInheritTypeInfo>();
        result->mName       = _readZTS();
        result->mOffset     = 0x0;
        result->mParentType = _readZTI();
        if (result->mName.empty() || result->mParentType.empty()) {
            spdlog::error("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", last());
            return nullptr;
        }
        return result;
    }
    case H("_ZTVN10__cxxabiv121__vmi_class_type_infoE"): {
        auto result   = std::make_unique<MultipleInheritTypeInfo>();
        result->mName = _readZTS();
        if (result->mName.empty()) {
            spdlog::error("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", last());
            return nullptr;
        }
        result->mAttribute = read<unsigned int>();
        auto baseCount     = read<unsigned int>();
        for (unsigned int idx = 0; idx < baseCount; idx++) {
            BaseClassInfo baseInfo;
            baseInfo.mName = _readZTI();
            if (baseInfo.mName.empty()) {
                spdlog::error("Failed to reading type info at {:#x}. [ABNORMAL_SYMBOL_VALUE]", last());
                return nullptr;
            }
            auto flag        = read<long long>();
            baseInfo.mOffset = (flag >> 8) & 0xFF;
            baseInfo.mMask   = flag & 0xFF;
            result->mBaseClasses.emplace_back(baseInfo);
        }
        return result;
    }
    default:
        // spdlog::error("Failed to reading type info at {:#x}. [UNKNOWN_INHERIT_TYPE]", beginAddr);
        return nullptr;
    }
}

void VTableReader::printDebugString(const VTable& pTable) {
    spdlog::info("VTable: {}", pTable.mName);
    for (auto& i : pTable.mSubTables) {
        spdlog::info("\tOffset: {:#x}", i.first);
        for (auto& j : i.second) {
            spdlog::info("\t\t{} ({:#x})", j.mSymbolName.has_value() ? "<unknown>" : *j.mSymbolName, j.mRVA);
        }
    }
}

void VTableReader::_prepareData() {
    if (!mIsValid) return;
    for (auto& symbol : mImage->symtab_symbols()) {
        if (symbol.name().starts_with("_ZTV")) {
            mPrepared.mVTableBegins.emplace(symbol.value());
        } else if (symbol.name().starts_with("_ZTI")) {
            mPrepared.mTypeInfoBegins.emplace(symbol.value());
        }
    }
    for (auto& relocation : mImage->dynamic_relocations()) {
        if (!relocation.has_symbol()) return;
        switch (H(relocation.symbol()->name().c_str())) {
        case H("_ZTVN10__cxxabiv117__class_type_infoE"):
        case H("_ZTVN10__cxxabiv120__si_class_type_infoE"):
        case H("_ZTVN10__cxxabiv121__vmi_class_type_infoE"):
            mPrepared.mTypeInfoBegins.emplace(relocation.address());
        }
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

METADUMPER_ELF_END
