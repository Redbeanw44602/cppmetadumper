//
// Created by RedbeanW on 2024/2/4.
//

#include "MachO.h"

METADUMPER_FORMAT_BEGIN

MachO::MachO(const std::string& pPath) : Loader(pPath) {
    auto fatBinary = LIEF::MachO::Parser::parse(pPath);
    if (!fatBinary) {
        spdlog::error("Failed to load mach-o image.");
        mIsValid = false;
        return;
    }
    if (fatBinary->size() > 1) {
        spdlog::error("FatBinary are not supported yet.");
        mIsValid = false;
        return;
    }
    mImage = fatBinary->take(0);
    _buildSymbolCache();
    _relocateReadonlyData();
}

uintptr_t MachO::getEndOfSections() const {
    uintptr_t ret = 0;
    for (auto& sec : mImage->sections()) {
        auto end = sec.virtual_address() + sec.size();
        if (ret < end) {
            ret = end;
        }
    }
    return ret;
}

size_t MachO::getGapInFront(uintptr_t pAddr) const {
    size_t ret;
    for (auto& segment : mImage->segments()) {
        auto begin = segment.virtual_address();
        ret        = begin - segment.file_offset();
        if (pAddr >= begin && pAddr < begin + segment.virtual_size()) {
            return ret;
        }
    }
    spdlog::error("An exception occurred during gap calculation!");
    return -1;
}

bool MachO::isInSection(uintptr_t pAddr, const std::string& pSecName) const {
    auto section = mImage->get_section(pSecName);
    return section && section->virtual_address() <= pAddr && (section->virtual_address() + section->size()) > pAddr;
}

LIEF::MachO::Symbol* MachO::lookupSymbol(uintptr_t pAddr) {
    if (mSymbolCache.mFromValue.contains(pAddr)) return mSymbolCache.mFromValue.at(pAddr);
    return nullptr;
}

LIEF::MachO::Symbol* MachO::lookupSymbol(const std::string& pName) {
    if (mSymbolCache.mFromName.contains(pName)) return mSymbolCache.mFromName.at(pName);
    return nullptr;
}

bool MachO::moveToSection(const std::string& pName) {
    auto section = mImage->get_section(pName);
    return section && move(section->virtual_address(), Begin);
}

void MachO::_relocateReadonlyData() {
    // TODO.
}

void MachO::_buildSymbolCache() {
    if (!mIsValid) return;

    for (auto& symbol : mImage->symbols()) {
        mSymbolCache.mFromName.try_emplace(symbol.name(), &symbol);
        mSymbolCache.mFromValue.try_emplace(symbol.value(), &symbol);
    }
}

METADUMPER_FORMAT_END
