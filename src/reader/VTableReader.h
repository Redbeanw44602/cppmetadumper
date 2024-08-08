//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "ElfReader.h"

#include <map>
#include <unordered_set>

enum class TypeInheritKind { None, Single, Multiple };

struct TypeInfo {
    std::string                           mName; // _ZTI...
    [[nodiscard]] virtual TypeInheritKind kind() const = 0;
};

struct NoneInheritTypeInfo : public TypeInfo {
    using TypeInfo::TypeInfo;
    [[nodiscard]] TypeInheritKind kind() const override { return TypeInheritKind::None; };
};

struct SingleInheritTypeInfo : public TypeInfo {
    using TypeInfo::TypeInfo;
    [[nodiscard]] TypeInheritKind kind() const override { return TypeInheritKind::Single; };
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
    unsigned int                  mAttribute;
    // bool mIsWeak;
    std::vector<BaseClassInfo> mBaseClasses;
};

struct VTableColumn {
    std::string mSymbolName;
    Elf64_Addr  mRVA{0};
};

struct VTable {
    std::string                                                    mName;     // _ZTV...
    std::optional<std::string>                                     mTypeName; // _ZTI...
    std::map<ptrdiff_t, std::vector<VTableColumn>, std::greater<>> mSubTables;
};

struct DumpVFTableResult {
    unsigned int        mTotal{0};
    unsigned int        mParsed{0};
    std::vector<VTable> mVFTable;
};

struct DumpTypeInfoResult {
    unsigned int                           mTotal{0};
    unsigned int                           mParsed{0};
    std::vector<std::unique_ptr<TypeInfo>> mTypeInfo;
};

class VTableReader : public ElfReader {
public:
    explicit VTableReader(const std::string& pPath);

    DumpVFTableResult     dumpVFTable();
    std::optional<VTable> readVTable();

    DumpTypeInfoResult        dumpTypeInfo();
    std::unique_ptr<TypeInfo> readTypeInfo();

    static void printDebugString(const VTable& pTable);
    static void printDebugString(const std::unique_ptr<TypeInfo>& pType);

private:
    void _prepareData();

    struct PreparedData {
        std::unordered_set<Elf64_Addr> mVTableBegins;
        std::unordered_set<Elf64_Addr> mTypeInfoBegins;
        std::unordered_set<Elf64_Addr> mLambdaBegins;
    } mPrepared;
};
