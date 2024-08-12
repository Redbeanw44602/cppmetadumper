//
// Created by RedbeanW on 2023/7/27.
//

#include "base/Base.h"

#include "reader/elf/VTableReader.h"

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

#include <fstream>

using JSON = nlohmann::json;

using namespace metadumper;

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

JSON read_vtable(elf::VTableReader& reader) {
    auto vftable = reader.dumpVFTable();
    spdlog::info(
        "Parsed vftable(s): {}/{}({:.4}%)",
        vftable.mParsed,
        vftable.mTotal,
        ((double)vftable.mParsed / (double)vftable.mTotal) * 100.0
    );
    return vftable.toJson();
}

JSON read_typeinfo(elf::VTableReader& reader) {
    auto types = reader.dumpTypeInfo();
    spdlog::info(
        "Parsed typeinfo(s): {}/{}({:.4}%)",
        types.mParsed,
        types.mTotal,
        ((double)types.mParsed / (double)types.mTotal) * 100.0
    );
    return types.toJson();
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

    elf::VTableReader reader(inputFileName);
    if (!reader.isValid()) return -1;

    spdlog::info("{:<12}{}", "Input file:", inputFileName);

    auto jsonVft = read_vtable(reader);
    auto jsonTyp = read_typeinfo(reader);

    save_to_json(outputFileBase + ".vftable.json", jsonVft);
    save_to_json(outputFileBase + ".typeinfo.json", jsonTyp);

    spdlog::info("All works done...");

    return 0;
}