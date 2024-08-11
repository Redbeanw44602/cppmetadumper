//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// clang-format off

#define METADUMPER_BEGIN namespace metadumper {
#define METADUMPER_END   }

#define METADUMPER_ELF_BEGIN    METADUMPER_BEGIN namespace elf {
#define METADUMPER_ELF_END      METADUMPER_END   }

#define METADUMPER_MACHO_BEGIN  METADUMPER_BEGIN namespace macho {
#define METADUMPER_MACHO_END    METADUMPER_END   }

#define METADUMPER_PE_BEGIN     METADUMPER_BEGIN namespace pe {
#define METADUMPER_PE_END       METADUMPER_END   }

// clang-format on
