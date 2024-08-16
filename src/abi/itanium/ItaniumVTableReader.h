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
        std::string SEGMENT_TEXT;
        std::string SEGMENT_DATA;
        std::string SEGMENT_READONLY_DATA;
        std::string PREFIX_VTABLE;
        std::string PREFIX_TYPEINFO;
        std::string SYM_CLASS_INFO;
        std::string SYM_SI_CLASS_INFO;
        std::string SYM_VMI_CLASS_INFO;
        std::string SYM_PURE_VFN;
    } _constant;

    struct PreparedData {
        std::unordered_set<uintptr_t> mVTableBegins;
        std::unordered_set<uintptr_t> mTypeInfoBegins;
        // Fake symbol mapping.
        // TODO: save memory...
        std::unordered_map<uintptr_t, std::string> mExternalSymbolPosition;
    } mPrepared;

    std::shared_ptr<Executable> mImage;
};

METADUMPER_ABI_ITANIUM_END
