//
// Created by RedbeanW on 2023/7/27.
//

#pragma once

#include "Exporter.h"

#include <fstream>
#include <set>
#include <elfio/elfio.hpp>

class Loader {
public:

    using Offset = int32_t;
    using Flag = int32_t;
    using LFlag = int64_t;

    enum InheritType {
        None,
        Single,
        Multiple
    };

    struct BaseClassInfo {
        std::string mName;
        LFlag mMask{};
        LFlag mOffset{};
        struct UniqueComparer {
            bool operator()(const std::unique_ptr<BaseClassInfo>& left, const std::unique_ptr<BaseClassInfo>& right) const {
                return left->mOffset < right->mOffset;
            }
        };
    };

    struct RTTI {
        explicit RTTI(ELFIO::Elf64_Addr beg, InheritType it)
        noexcept : mAddress(beg), mInherit(it) {};
        ELFIO::Elf64_Addr mAddress;
        InheritType mInherit{};
        std::string mName;
        Flag mAttribute{};
        bool mWeak{};
        std::set<std::unique_ptr<BaseClassInfo>, BaseClassInfo::UniqueComparer> mParents;
    };

    using WholeRTTIMap = std::unordered_map<ELFIO::Elf64_Addr, RTTI>;

    struct LoadResult {
        enum {
            Ok,
            OpenFileFailed,
            FailToParse,
            UnsupportedArchitecture
        } mResult;
        [[nodiscard]] std::string getErrorMsg() const;
    };

    struct DumpVTableResult {
        enum {
            Ok,
            SectionNotFound,
            RebuildRelativeReadOnlyDataFailed,
        } mResult;
        WholeRTTIMap mRTTI;
        [[nodiscard]] std::string getErrorMsg() const;
    };

    struct DumpSymbolResult {
        enum {
            Ok
        } mResult;
        // std::set<>
    };

    struct ExportVTableArguments {
        bool mRTTI{};
        bool mVTable{};
        bool mDumpSegment{};
    };

    explicit Loader(std::string& path)
        noexcept : mPath(path) {};
    Loader() = delete;

    ~Loader();

    LoadResult load();

    [[nodiscard]] DumpVTableResult dumpVTable(ExportVTableArguments args);
    [[maybe_unused]] DumpSymbolResult dumpSymbols();

private:

    std::string mPath;
    std::ifstream mFileStream;

    ELFIO::elfio mImage;
    ELFIO::Elf64_Addr mEndOfSection{};

    [[maybe_unused]] uint64_t _getGapInFront(ELFIO::Elf64_Addr address);

    [[nodiscard]] std::stringstream _rebuildRelativeData(ELFIO::section* relroSection, bool saveDump = false);

    [[nodiscard]] ELFIO::Elf64_Addr _getEndOfSectionAddress();

    [[nodiscard]] ELFIO::section* _getSection(const std::string& name);
    [[nodiscard]] ELFIO::section* _getSection(ELFIO::Elf_Word type);

    [[nodiscard]] WholeRTTIMap _getRTTIStarts();

    [[nodiscard]] uint64_t _readOneRTTI(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr starts, WholeRTTIMap& allRttiInfo);

    void _readCString(int64_t begin, std::string& result); // max length 2048.

};