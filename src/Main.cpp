//
// Created by RedbeanW on 2023/7/27.
//

#include "Base.h"
#include "reader/VTableReader.h"

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

using JSON = nlohmann::json;

int main(int argc, char* argv[]) {

    argparse::ArgumentParser program("VTable Exporter", "2.0.0");

    auto logger = spdlog::stdout_color_st("Exporter");
    logger->set_pattern("[%T.%e] [%n] [%^%l%$] %v");
    spdlog::set_default_logger(logger);

    // clang-format off
    program.add_argument("target")
        .help("Path to a valid ELF file.")
        .required();
    program.add_argument("-o", "--output")
        .help("Path to save the result, in JSON format.")
        .required();
    // clang-format on

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        spdlog::error(e.what());
        return -1;
    }

    auto input  = program.get<std::string>("target");
    auto output = program.get<std::string>("-o");
    if (output.ends_with(".json")) {
        output.erase(output.size() - 5, 5);
    }
    output += ".json";

    VTableReader reader(input);
    if (!reader.isValid()) {
        return 1;
    }

    // Virtual Functions
    JSON jsonVft;
    auto resultVft = reader.dumpVFTable();
    for (auto& i : resultVft.mVFTable) {
        auto subTables = JSON::array();
        for (auto& j : i.mSubTables) {
            auto entities = JSON::array();
            for (auto& k : j.second) {
                entities.emplace_back(JSON{
                    {"symbol", k.mSymbolName},
                    {"rva",    k.mRVA       }
                });
            }
            subTables.emplace_back(JSON{
                {"offset",   j.first },
                {"entities", entities}
            });
        }
        jsonVft[i.mName] = JSON{
            {"sub_tables", subTables}
        };
        if (i.mTypeName) {
            jsonVft[i.mName]["type_name"] = *i.mTypeName;
        } else {
            jsonVft[i.mName]["type_name"] = {};
        }
    }
    spdlog::info(
        "Parsed vftable(s): {}/{}({:.4}%)",
        resultVft.mParsed,
        resultVft.mTotal,
        ((double)resultVft.mParsed / (double)resultVft.mTotal) * 100.0
    );

    // TypeInfos
    JSON jsonTyp;
    auto resultTyp = reader.dumpTypeInfo();
    for (auto& type : resultTyp.mTypeInfo) {
        if (!type) continue;
        switch (type->kind()) {
        case TypeInheritKind::None: {
            auto typeInfo            = (NoneInheritTypeInfo*)type.get();
            jsonTyp[typeInfo->mName] = {
                {"inherit_type", "None"}
            };
            break;
        }
        case TypeInheritKind::Single: {
            auto typeInfo            = (SingleInheritTypeInfo*)type.get();
            jsonTyp[typeInfo->mName] = {
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
            jsonTyp[typeInfo->mName] = {
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

    std::ofstream file(output, std::ios::trunc);
    if (file.is_open()) {
        file << JSON{
            {"vtable",   jsonVft},
            {"typeinfo", jsonTyp}
        }.dump(4);
        file.close();
        spdlog::info("Results have been saved to: {}", output);
    } else {
        spdlog::error("Failed to open file!");
        return 1;
    }

    spdlog::info("All works done...");

    return 0;
}