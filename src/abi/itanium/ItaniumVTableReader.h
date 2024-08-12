//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "base/Base.h"
#include "format/ELF.h"

#include "ItaniumVTable.h"

#include <unordered_set>

METADUMPER_ABI_ITANIUM_BEGIN

struct DumpVFTableResult {
    unsigned int        mTotal{};
    unsigned int        mParsed{};
    std::vector<VTable> mVFTable;
    nlohmann::json      toJson() const;
};

struct DumpTypeInfoResult {
    unsigned int                           mTotal{};
    unsigned int                           mParsed{};
    std::vector<std::unique_ptr<TypeInfo>> mTypeInfo;
    nlohmann::json                         toJson() const;
};

class ItaniumVTableReader : public format::ELF {
public:
    explicit ItaniumVTableReader(const std::string& pPath) : ELF(pPath) { _prepareData(); };

    DumpVFTableResult     dumpVFTable();
    std::optional<VTable> readVTable();

    DumpTypeInfoResult        dumpTypeInfo();
    std::unique_ptr<TypeInfo> readTypeInfo();

    static void printDebugString(const VTable& pTable);
    static void printDebugString(const std::unique_ptr<TypeInfo>& pType);

private:
    void _prepareData();

    std::string _readZTS();
    std::string _readZTI();

    struct PreparedData {
        std::unordered_set<uintptr_t> mVTableBegins;
        std::unordered_set<uintptr_t> mTypeInfoBegins;
    } mPrepared;
};

METADUMPER_ABI_ITANIUM_END
