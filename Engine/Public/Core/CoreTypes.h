#pragma once

#include <cstdint>

// Portable fixed-width integer types, as required by the Epic C++ Coding Standard.
using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

// Symbol visibility for module DLLs. The build defines <MODULE>_API (for example ENGINE_API)
// as DLLEXPORT while compiling that module and as DLLIMPORT for every module that links to it.
#if defined(_MSC_VER)
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLIMPORT __attribute__((visibility("default")))
#endif
