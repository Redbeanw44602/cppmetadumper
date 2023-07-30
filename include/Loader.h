//
// Created by RedbeanW on 2023/7/27.
//

#pragma once

#include "Exporter.h"

#include <fstream>
#include <set>
#include <unordered_set>
#include <elfio/elfio.hpp>

class Loader {
public:

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
            bool operator()(const BaseClassInfo& left, const BaseClassInfo& right) const {
                return left.mOffset < right.mOffset;
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
        std::set<BaseClassInfo, BaseClassInfo::UniqueComparer> mParents;
    };

    struct Symbol {
        uint32_t mIndex{};
        ELFIO::Elf64_Addr mAddress{};
        std::string mName;
        struct Comparer {
            bool operator()(const Symbol& left, const Symbol& right) const {
                return left.mIndex < right.mIndex;
            }
        };
    };

    struct VTable {
        explicit VTable(std::string name)
            noexcept : mName(std::move(name)) {};
        LFlag mOffset{};
        std::string mName;
        std::set<Symbol, Symbol::Comparer> mEntries;
    };

    using WholeRTTIMap = std::unordered_map<ELFIO::Elf64_Addr, RTTI>;
    using WholeVTableMap = std::unordered_map<ELFIO::Elf64_Addr ,VTable>;

    struct SymbolCache {
        std::unordered_map<ELFIO::Elf64_Addr, std::string> mByValue;

        // for external symbol, emm...
        std::unordered_map<ELFIO::Elf64_Addr, std::string> mByAddress;
    };

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
            NoSymTab
        } mResult;
        WholeRTTIMap mRTTI;
        WholeVTableMap mVTable;
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

    struct Sections {
        ELFIO::section* mRELA{};
        ELFIO::section* mSYMTAB{};
        ELFIO::section* mRELRO{};
    } mPreparedSections;

    [[maybe_unused]] uint64_t _getGapInFront(ELFIO::Elf64_Addr address);

    [[nodiscard]] std::stringstream _rebuildRelativeData(ELFIO::section* relroSection, bool saveDump = false);
    [[nodiscard]] SymbolCache _buildSymbolCache();

    [[nodiscard]] ELFIO::Elf64_Addr _getEndOfSectionAddress();

    [[nodiscard]] WholeRTTIMap _getRTTIStarts();
    [[nodiscard]] WholeVTableMap _getVTableStarts();

    [[nodiscard]] uint64_t _readOneRTTI(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr starts, WholeRTTIMap& allRttiInfo);
    [[nodiscard]] uint64_t _readOneVTable(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr starts, WholeRTTIMap& allRttiInfo, const SymbolCache& symCache, WholeVTableMap& allVTableInfo);
    [[nodiscard]] std::string _readOneTypeName(std::stringstream& relroData, uint64_t relroBase, ELFIO::Elf64_Addr begin, bool keepPtrPos = false);

    void _readCString(int64_t begin, std::string& result); // max length 2048.

};