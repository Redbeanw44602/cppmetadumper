//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include "Reader.h"

#include <unordered_set>
#include <map>

struct RTTI {

};

struct VTableColumn {
    std::string mSymbolName;
    Elf64_Addr  mRVA {0};
};

struct VTable {
    std::string mName;
    std::map<ptrdiff_t, std::vector<VTableColumn>> mSubTables;
    std::optional<RTTI> mRTTI;
};

class VTableReader : private Reader {
public:

    explicit VTableReader(const std::string& pPath);

    std::vector<VTable> dumpVTable();

    VTable readVTable(bool pIsSubRead = false);

    std::optional<RTTI> parseRTTI();

private:

    void _prepareData();

    struct PreparedData {
        std::unordered_set<Elf64_Addr> mVTableBegins;
    } mPrepared;

};