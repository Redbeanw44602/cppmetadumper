//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "ItaniumVTable.h"

#include "base/Base.h"
#include "base/Executable.h"

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

class ItaniumVTableReader {
public:
    explicit ItaniumVTableReader(const std::shared_ptr<Executable>& image);

    DumpVFTableResult  dumpVFTable();
    DumpTypeInfoResult dumpTypeInfo();

    static void printDebugString(const VTable& pTable);
    static void printDebugString(const std::unique_ptr<TypeInfo>& pType);

private:
    void _prepareData();

    std::optional<VTable>     readVTable();
    std::unique_ptr<TypeInfo> readTypeInfo();

    std::string _readZTS();
    std::string _readZTI();

    void _initFormatConstants();

    struct FormatConstants {
        std::string _segment_text;
        std::string _segment_data;
        std::string _prefix_vtable;
        std::string _prefix_typeinfo;
        std::string _sym_class_info;
        std::string _sym_si_class_info;
        std::string _sym_vmi_class_info;
    } _constant;

    struct PreparedData {
        std::unordered_set<uintptr_t> mVTableBegins;
        std::unordered_set<uintptr_t> mTypeInfoBegins;
    } mPrepared;

    std::shared_ptr<Executable> mImage;
};

METADUMPER_ABI_ITANIUM_END
