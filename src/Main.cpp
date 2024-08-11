//
// Created by RedbeanW on 2023/7/27.
//

#include "base/Base.h"

#include "reader/elf/VTableReader.h"

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

using JSON = nlohmann::json;

using namespace metadumper;
using namespace metadumper::elf;

std::tuple<std::string, std::string> init_program(int argc, char* argv[]) {
    argparse::ArgumentParser args("cppmetadumper", "2.0.0");

    // clang-format off
    args.add_argument("target")
        .help("Path to a valid executable.")
        .required();
    args.add_argument("-o", "--output")
        .help("Path to save the result, in JSON format.")
        .required();
    // clang-format on

    args.parse_args(argc, argv);

    return std::make_tuple(args.get<std::string>("target"), args.get<std::string>("-o"));
}

void init_logger() {
    auto logger = spdlog::stdout_color_st("cppmetadumper");
    logger->set_pattern("[%T.%e %^%l%$] %v");
#ifndef NDEBUG
    logger->set_level(spdlog::level::debug);
#endif
    spdlog::set_default_logger(logger);
}

JSON read_vtable(VTableReader& reader) {
    JSON result;
    auto resultVft = reader.dumpVFTable();
    for (auto& i : resultVft.mVFTable) {
        auto subTables = JSON::array();
        for (auto& j : i.mSubTables) {
            auto entities = JSON::array();
            for (auto& k : j.second) {
                entities.emplace_back(JSON{
                    {"symbol", k.mSymbolName.has_value() ? JSON(*k.mSymbolName) : JSON{}},
                    {"rva",    k.mRVA                                                   }
                });
            }
            subTables.emplace_back(JSON{
                {"offset",   j.first },
                {"entities", entities}
            });
        }
        result[i.mName] = JSON{
            {"sub_tables", subTables}
        };
        if (i.mTypeName) {
            result[i.mName]["type_name"] = *i.mTypeName;
        } else {
            result[i.mName]["type_name"] = {};
        }
    }
    spdlog::info(
        "Parsed vftable(s): {}/{}({:.4}%)",
        resultVft.mParsed,
        resultVft.mTotal,
        ((double)resultVft.mParsed / (double)resultVft.mTotal) * 100.0
    );
    return result;
}

JSON read_typeinfo(VTableReader& reader) {
    JSON result;
    auto resultTyp = reader.dumpTypeInfo();
    for (auto& type : resultTyp.mTypeInfo) {
        if (!type) continue;
        switch (type->kind()) {
        case TypeInheritKind::None: {
            auto typeInfo           = (NoneInheritTypeInfo*)type.get();
            result[typeInfo->mName] = {
                {"inherit_type", "None"}
            };
            break;
        }
        case TypeInheritKind::Single: {
            auto typeInfo           = (SingleInheritTypeInfo*)type.get();
            result[typeInfo->mName] = {
                {"inherit_type", "Single"             },
                {"parent_type",  typeInfo->mParentType},
                {"offset",       typeInfo->mOffset    }
            };
            break;
        }
        case TypeInheritKind::Multiple: {
            auto typeInfo    = (MultipleInheritTypeInfo*)type.get();
            auto baseClasses = JSON::array();
            for (auto& base : typeInfo->mBaseClasses) {
                baseClasses.emplace_back(JSON{
                    {"offset", base.mOffset},
                    {"name",   base.mName  },
                    {"mask",   base.mMask  }
                });
            }
            result[typeInfo->mName] = {
                {"inherit_type", "Multiple"          },
                {"attribute",    typeInfo->mAttribute},
                {"base_classes", baseClasses         }
            };
            break;
        }
        }
    }
    spdlog::info(
        "Parsed typeinfo(s): {}/{}({:.4}%)",
        resultTyp.mParsed,
        resultTyp.mTotal,
        ((double)resultTyp.mParsed / (double)resultTyp.mTotal) * 100.0
    );
    return result;
}

void save_to_json(const std::string& fileName, const JSON& result) {
    std::ofstream file(fileName, std::ios::trunc);
    if (file.is_open()) {
        file << result.dump(4);
        file.close();
        spdlog::info("Results have been saved to: {}", fileName);
    } else {
        spdlog::error("Failed to open {}!", fileName);
        return;
    }
}

int main(int argc, char* argv[]) {

    init_logger();

    std::string inputFileName, outputFileBase;
    try {
        std::tie(inputFileName, outputFileBase) = init_program(argc, argv);
    } catch (const std::runtime_error& e) {
        spdlog::error(e.what());
        return -1;
    }

    if (outputFileBase.ends_with(".json")) {
        outputFileBase.erase(outputFileBase.size() - 5, 5);
    }

    VTableReader reader(inputFileName);
    if (!reader.isValid()) return -1;

    spdlog::info("{:<12}{}", "Input file:", inputFileName);
    spdlog::info("{:<12}{} for {}", "Format:", "ELF64", arch2str(reader.getArchitecture()));

    auto jsonVft = read_vtable(reader);
    auto jsonTyp = read_typeinfo(reader);

    save_to_json(outputFileBase + ".vftable.json", jsonVft);
    save_to_json(outputFileBase + ".typeinfo.json", jsonTyp);

    spdlog::info("All works done...");

    return 0;
}