//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "base/Base.h"

#include "ELF.h"

#include <map>
#include <unordered_set>

#include <nlohmann/json.hpp>

METADUMPER_ELF_BEGIN

enum class TypeInheritKind { None, Single, Multiple };

struct TypeInfo {
    std::string                           mName; // _ZTI...
    [[nodiscard]] virtual TypeInheritKind kind() const   = 0;
    [[nodiscard]] virtual nlohmann::json  toJson() const = 0;
};

struct NoneInheritTypeInfo : public TypeInfo {
    using TypeInfo::TypeInfo;
    [[nodiscard]] TypeInheritKind kind() const override { return TypeInheritKind::None; };
    [[nodiscard]] nlohmann::json  toJson() const override;
};

struct SingleInheritTypeInfo : public TypeInfo {
    using TypeInfo::TypeInfo;
    [[nodiscard]] TypeInheritKind kind() const override { return TypeInheritKind::Single; };
    [[nodiscard]] nlohmann::json  toJson() const override;
    std::string                   mParentType; // _ZTI...
    // bool mIsWeak;
    ptrdiff_t mOffset;
};

struct BaseClassInfo {
    enum Mask { Virtual = 0x1, Public = 0x2, Offset = 0x8 };
    std::string  mName; // _ZTI...
    ptrdiff_t    mOffset;
    unsigned int mMask;
};

struct MultipleInheritTypeInfo : public TypeInfo {
    using TypeInfo::TypeInfo;
    [[nodiscard]] TypeInheritKind kind() const override { return TypeInheritKind::Multiple; };
    [[nodiscard]] nlohmann::json  toJson() const override;
    unsigned int                  mAttribute;
    // bool mIsWeak;
    std::vector<BaseClassInfo> mBaseClasses;
};

struct VTableColumn {
    std::optional<std::string> mSymbolName;
    uintptr_t                  mRVA{};
    nlohmann::json             toJson() const;
};

struct VTable {
    std::string                                                    mName;     // _ZTV...
    std::optional<std::string>                                     mTypeName; // _ZTI...
    std::map<ptrdiff_t, std::vector<VTableColumn>, std::greater<>> mSubTables;
    nlohmann::json                                                 toJson() const;
};

struct DumpVFTableResult {
    unsigned int        mTotal{0};
    unsigned int        mParsed{0};
    std::vector<VTable> mVFTable;
    nlohmann::json      toJson() const;
};

struct DumpTypeInfoResult {
    unsigned int                           mTotal{0};
    unsigned int                           mParsed{0};
    std::vector<std::unique_ptr<TypeInfo>> mTypeInfo;
    nlohmann::json                         toJson() const;
};

class VTableReader : public ELF {
public:
    explicit VTableReader(const std::string& pPath) : ELF(pPath) { _prepareData(); };

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

METADUMPER_ELF_END
