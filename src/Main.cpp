//
// Created by RedbeanW on 2023/7/27.
//

#include "Base.h"
#include "reader/VTableReader.h"

#include <argparse/argparse.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char* argv[]) {

    argparse::ArgumentParser program("VTable Exporter", "1.1.0");

    auto logger = spdlog::stdout_color_st("Exporter");
    logger->set_pattern("[%T.%e] [%n] [%^%l%$] %v");
    spdlog::set_default_logger(logger);

    program.add_argument("target")
        .help("Path to a valid ELF file.")
        .required();
    program.add_argument("-o", "--output")
        .help("Path to save the result, in JSON format.")
        .required();
    program.add_argument("--dump-segment")
        .help("Save the .data.rel.ro segment as segment.dump.")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--no-rva")
        .help("I don't care about the RVA data for that image.")
        .default_value(false)
        .implicit_value(true);
#if 0
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
#endif
    VTableReader reader("bedrock_server_symbols.debug");
    reader.dumpVTable();


    spdlog::info("All works done...");


}