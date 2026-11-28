#pragma once

#include "Core/CoreTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

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

	/** Returns the path that UTF-8 text names: the opposite of PathToUtf8. */
	ENGINE_API static std::filesystem::path PathFromUtf8(std::string_view Text);

	/**
	 * Returns true if two names of files, or two paths written alike, are the same where capitals are not told
	 * apart: equal but for the case of their letters, as Windows compares the names of files. On Windows this is the
	 * system's own comparison, for every letter that has a capital; elsewhere it is the letters A to Z.
	 *
	 * @param First UTF-8 text.
	 * @param Second UTF-8 text.
	 */
	ENGINE_API static bool IsSameNameIgnoringCase(std::string_view First, std::string_view Second);

	/**
	 * Returns what the operating system says about an error of the file system, as words to put into a sentence:
	 * without the full stop, the spaces and the line break that some systems end theirs with.
	 */
	ENGINE_API static std::string DescribeError(const std::error_code& Error);
};
