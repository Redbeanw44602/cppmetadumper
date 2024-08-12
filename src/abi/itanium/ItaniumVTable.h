#pragma once

#include "base/Base.h"

#include <nlohmann/json.hpp>

METADUMPER_ABI_ITANIUM_BEGIN

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

METADUMPER_ABI_ITANIUM_END