//
// Created by RedbeanW on 2024/1/19.
//

#include "VTableReader.h"

VTableReader::VTableReader(const std::string& pPath) : Reader(pPath) {
    _prepareData();
}

VTable VTableReader::readVTable(bool pIsSubRead) {
    VTable result;
    auto vtSym = lookupSymbol(cur());
    if (!pIsSubRead) {
        if (!vtSym || vtSym->mName.starts_with("_ZTV")) {
            spdlog::error("err reading vtable at {:#x}", cur());
        }
        result.mName = vtSym->mName;
    } else {
        if (!vtSym || vtSym->mName.starts_with("_ZTI")) {
            spdlog::error("err reading vtable at {:#x}", cur());
        }
    }
    auto offsetToThis = read<int64_t>();
    if (offsetToThis > 0) { // offset to this
        spdlog::error("err reading vtable at {:#x}", cur() - sizeof(int64_t));
    }
    result.mSubTables.emplace(offsetToThis, std::vector<VTableColumn> {});
    auto& me = result.mSubTables.at(offsetToThis);
    size_t count = 0;
    while (true) {
        count++;
        auto curValue = read<int64_t>();
        //spdlog::warn("val: {:#x}", curValue);
        if (curValue <= 0) { // Is sub table(like virtual thunk...), merge it.
            move(-8);
            auto subResult = readVTable(true);
            for (auto& i : subResult.mSubTables) { // FIXME: Can't get sub-table!
                result.mSubTables.emplace(i); // move it!
            }
            break;
        }
        if (curValue == 0 && count != 1) { // Is next table.
            break;
        }
        auto sym = lookupSymbol(curValue);
        if (!sym || (count > 2 && sym->mName.starts_with("_ZTI"))) {
            break;
        }
        me.emplace_back(VTableColumn {sym->mName, sym->mValue});
    }
    if (count <= 1) {
        result.mSubTables.clear();
    }
    return result;
}

std::optional<RTTI> VTableReader::parseRTTI() {
    // Reference:
    // https://itanium-cxx-abi.github.io/cxx-abi/abi.html#rtti-layout
#if 0
    auto& ins = allRttiInfo.at(starts);
    unsigned char buffer[8]; // tmp
    ins.mName = _readOneTypeName(relroData, relroBase, starts);
    switch (ins.mInherit) {
        case None:
            // done.
            break;
        case Single: {
            relroData.read((char*)buffer, 8);
            BaseClassInfo base;
            auto strAddr = FromBytes<uint64_t>(buffer);
            if (strAddr >= mEndOfSection) {
                ins.mWeak = true;
                break;
            }
            base.mName = _readOneTypeName(relroData, relroBase, strAddr, true);
            base.mOffset = 0x0;
            ins.mParents.emplace(base);
            break;
        }
        case Multiple: {
            relroData.read((char*)buffer, 4);
            ins.mAttribute = FromBytes<Flag>(buffer);
            relroData.read((char*)buffer, 4); // read base class count.
            auto baseClassCount = FromBytes<int32_t>(buffer);
            for (int32_t i = 0; i < baseClassCount; i++) {
                relroData.read((char*)buffer, 8); // read base class' type name.
                BaseClassInfo base;
                auto strAddr = FromBytes<uint64_t>(buffer);
                if (strAddr >= mEndOfSection) {
                    ins.mWeak = true;
                    break;
                }
                base.mName = _readOneTypeName(relroData, relroBase, strAddr, true);
                relroData.read((char*)buffer, 8); // read base class' attributes
                auto attribute = FromBytes<Flag>(buffer);
                base.mOffset = (attribute >> 8) & 0xFF;
                base.mMask = attribute & 0xFF;
                ins.mParents.emplace(base);
            }
            break;
        }
        default:
            spdlog::warn("Unknown inherit type detected when reading rtti information!");
            break;
    }
    return (uint64_t)relroData.tellg() - (starts - relroBase);
#endif
    return std::nullopt;
}

void VTableReader::_prepareData() {

    auto result = forEachSymbols([this](const Symbol& pSym) {
        if (pSym.mName.starts_with("_ZTV")) {
            mPrepared.mVTableBegins.emplace(pSym.mValue);
        }
    });

    if (!result) {
        mState.setError("No symbols found in this image!");
    }

}

std::vector<VTable> VTableReader::dumpVTable() {
    std::vector<VTable> result;
    for (auto& i : mPrepared.mVTableBegins) {
        move(i, Begin);
        result.emplace_back(readVTable());
    }
    return result;
}
