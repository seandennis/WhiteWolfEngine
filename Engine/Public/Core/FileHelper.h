#pragma once

#include "Core/CoreTypes.h"

#include <filesystem>
#include <string>
#include <string_view>

/** Reads and writes whole text files. */
class FFileHelper
{
public:
	/**
	 * Reads a file into a string, byte for byte.
	 *
	 * @param OutText Receives the content. Unchanged on failure.
	 * @param OutError Receives the reason on failure, including the path.
	 * @return True if the file was read.
	 */
	ENGINE_API static bool LoadFileToString(
		const std::filesystem::path& Path, std::string& OutText, std::string& OutError);

	/**
	 * Writes a string to a file, byte for byte, creating the file's directory if needed.
	 *
	 * The text is written to a temporary file beside the target, which then replaces the target in one step. A
	 * failure, or a crash part-way through, therefore leaves an existing file as it was instead of half-written.
	 *
	 * @param OutError Receives the reason on failure, including the path.
	 * @return True if the file now holds Text.
	 */
	ENGINE_API static bool SaveStringToFile(
		std::string_view Text, const std::filesystem::path& Path, std::string& OutError);

	/** Returns a path as UTF-8 text, for messages and for saving in files. */
	ENGINE_API static std::string PathToUtf8(const std::filesystem::path& Path);
};
