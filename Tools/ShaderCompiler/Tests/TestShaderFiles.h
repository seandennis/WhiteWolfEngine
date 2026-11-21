#pragma once

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

/** The first word of every SPIR-V module. */
inline constexpr unsigned int SpirvMagicNumber = 0x07230203;

/** A vertex shader that compiles. */
inline constexpr std::string_view ValidVertexShader = "#version 460\n"
													  "layout(location = 0) in vec3 Position;\n"
													  "void main()\n"
													  "{\n"
													  "\tgl_Position = vec4(Position, 1.0);\n"
													  "}\n";

/** A fragment shader that compiles. */
inline constexpr std::string_view ValidFragmentShader = "#version 460\n"
														"layout(location = 0) out vec4 Color;\n"
														"void main()\n"
														"{\n"
														"\tColor = vec4(1.0, 0.5, 0.25, 1.0);\n"
														"}\n";

/** Writes a text file for a test, creating its directory. */
inline void WriteTestFile(const std::filesystem::path& Path, std::string_view Text)
{
	std::filesystem::create_directories(Path.parent_path());
	std::ofstream Stream(Path, std::ios::binary);
	Stream << Text;
	REQUIRE(Stream.good());
}

/** Reads a text file a test expects to exist. */
inline std::string ReadTestFile(const std::filesystem::path& Path)
{
	std::ifstream Stream(Path, std::ios::binary);
	REQUIRE(Stream.good());
	return std::string(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
}
