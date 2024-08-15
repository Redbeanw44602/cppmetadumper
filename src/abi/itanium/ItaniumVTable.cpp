#include "ItaniumVTable.h"

using JSON = nlohmann::json;

METADUMPER_ABI_ITANIUM_BEGIN

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
        {"sub_tables", subTables                                        },
        {"type_name",  mTypeName.has_value() ? JSON(*mTypeName) : JSON{}}
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

METADUMPER_ABI_ITANIUM_END
