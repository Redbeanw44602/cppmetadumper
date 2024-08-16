//
// Created by RedbeanW on 2024/1/19.
//

#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 1024

// clang-format off

#define METADUMPER_BEGIN namespace metadumper {
#define METADUMPER_END   }

// base 

#define METADUMPER_ABI_BEGIN    METADUMPER_BEGIN namespace abi {
#define METADUMPER_ABI_END      METADUMPER_END   }

#define METADUMPER_FORMAT_BEGIN METADUMPER_BEGIN namespace format {
#define METADUMPER_FORMAT_END   METADUMPER_END   }

#define METADUMPER_UTIL_BEGIN   METADUMPER_BEGIN namespace util {
#define METADUMPER_UTIL_END     METADUMPER_END }

// string

#define METADUMPER_UTIL_STRING_BEGIN   METADUMPER_UTIL_BEGIN namespace string {
#define METADUMPER_UTIL_STRING_END     METADUMPER_UTIL_END }

// abi

#define METADUMPER_ABI_ITANIUM_BEGIN METADUMPER_ABI_BEGIN namespace itanium {
#define METADUMPER_ABI_ITANIUM_END   METADUMPER_ABI_END   }

#define METADUMPER_ABI_MSVC_BEGIN    METADUMPER_ABI_BEGIN namespace msvc {
#define METADUMPER_ABI_MSVC_END      METADUMPER_ABI_END   }

// clang-format on
