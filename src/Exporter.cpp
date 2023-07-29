//
// Created by RedbeanW on 2023/7/27.
//

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

#include "Exporter.h"
#include "Loader.h"

using json = nlohmann::json;

int main(int argc, char* argv[]) {

    argparse::ArgumentParser program("VTable Exporter", "1.0.0");

    auto logger = spdlog::stdout_color_st("Exporter");
    logger->set_pattern("[%T.%e] [%n] [%^%l%$] %v");
    spdlog::set_default_logger(logger);

    program.add_argument("target")
        .help("Path to a valid ELF file.")
        .required();
    program.add_argument("-o", "--output")
        .help("Path to save the result, in JSON format.")
        .required();
    // program.add_argument("-s", "--export-all-symbols")
    //    .help("Extract symbol information from .symtab/.dynsym.")
    //    .default_value(false);
    program.add_argument("--export-rtti")
        .help("Export all RTTI information from ELF file.")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--export-vtable")
        .help("Export all virtual function information from the ELF file.")
        .default_value(false)
        .implicit_value(true);


    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        spdlog::error(e.what());
        return -1;
    }

    auto input = program.get<std::string>("target");
    auto output = program.get<std::string>("-o");
    if (output.ends_with(".json")) {
        output.erase(output.size() - 5, 5);
    }

    Loader loader(input);
    auto loadResult = loader.load();
    if (loadResult.mResult != Loader::LoadResult::Ok) {
        spdlog::error(loadResult.getErrorMsg());
        return -1;
    }
    spdlog::info(loadResult.getErrorMsg());

    Loader::ExportVTableArguments args;
    args.mRTTI = program["--export-rtti"] == true;
    args.mVTable = program["--export-vtable"] == true;

    Loader::DumpVTableResult dumpVTableResult;

    if (args.mRTTI || args.mVTable) {
        dumpVTableResult = loader.dumpVTable(args);
        if (dumpVTableResult.mResult != Loader::DumpVTableResult::Ok) {
            spdlog::error(dumpVTableResult.getErrorMsg());
            return -1;
        }
        spdlog::info(dumpVTableResult.getErrorMsg());
    }

    // Save result.
    if (args.mRTTI) {
        spdlog::info("Building json result (RTTI)...");
        json data;
        for (auto& i : dumpVTableResult.mRTTI) {
            auto& item = i.second;
            data[item.mName] = {
                    {"rva", item.mAddress},
                    {"attribute", item.mAttribute},
                    {"inherit_type", item.mInherit},
                    {"weak", item.mWeak}
            };
            if (item.mInherit == Loader::InheritType::Single
                || item.mInherit == Loader::InheritType::Multiple) {
                auto index = 0;
                data[item.mName]["parents"] = json::array();
                for (auto& j : item.mParents) {
                    data[item.mName]["parents"][index]["name"] = j->mName;
                    if (item.mInherit == Loader::InheritType::Multiple) {
                        data[item.mName]["parents"][index]["offset"] = j->mOffset;
                        data[item.mName]["parents"][index]["mask"] = j->mMask;
                    }
                    index++;
                }
            }
        }

        std::ofstream ofs(output + ".rtti.json", std::ios::out);
        if (!ofs.is_open()) {
            spdlog::error("Unable to open saving file!");
            return -1;
        }

        try {
            ofs << data.dump(4);
        } catch(const std::exception& e) {
            spdlog::error(e.what());
        }
        ofs.close();
    }

    spdlog::info("All works done.");


}