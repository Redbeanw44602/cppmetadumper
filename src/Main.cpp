#include "base/Base.h"

#include <argparse/argparse.hpp>
#include <fstream>

#include "format/ELF.h"
#include "format/MachO.h"
#include "util/MagicHelper.h"

#include "abi/itanium/ItaniumVTableReader.h"

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

JSON read_vtable(abi::itanium::ItaniumVTableReader& reader) {
    auto vftable = reader.dumpVFTable();
    spdlog::info(
        "Parsed vftable(s): {}/{}({:.4}%)",
        vftable.mParsed,
        vftable.mTotal,
        ((double)vftable.mParsed / (double)vftable.mTotal) * 100.0
    );
    return vftable.toJson();
}

JSON read_typeinfo(abi::itanium::ItaniumVTableReader& reader) {
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
    if (result.empty()) return;
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

    // setup I/O file name.

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

    spdlog::info("{:<12}{}", "Input file:", inputFileName);

    // judge fileType and processing.

    MagicHelper magic(inputFileName);
    if (!magic.isValid()) {
        spdlog::error("Unable to load input file.");
        return -1;
    }

    std::shared_ptr<Executable> image;

    switch (magic.judgeFileType()) {
    case Magic::ELF:
        image = std::make_shared<format::ELF>(inputFileName);
        break;
    case Magic::MACHO_64:
        image = std::make_shared<format::MachO>(inputFileName);
        break;
    case Magic::PE:
    case Magic::MACHO_32:
    case Magic::UNKNOWN:
    default:
        spdlog::error("Unsupported file type.");
        return -1;
    }

    if (!image->isValid()) return -1;

    abi::itanium::ItaniumVTableReader reader(image);

    try {
        auto jsonVftable = read_vtable(reader);
        auto jsonTypes   = read_typeinfo(reader);
        save_to_json(outputFileBase + ".vftable.json", jsonVftable);
        save_to_json(outputFileBase + ".typeinfo.json", jsonTypes);
    } catch (const std::runtime_error& e) {
        spdlog::error(e.what());
        return -1;
    }

    spdlog::info("All works done...");

    return 0;
}