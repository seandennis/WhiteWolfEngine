#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** File operations shared by the build tools, which run before the engine exists and so cannot use its helpers. */
class FToolFiles
{
public:
	/** Returns a path as UTF-8 text with forward slashes, which is how the tools print paths and write #includes. */
	static std::string ToUtf8(const std::filesystem::path& Path);

	/**
	 * Collects every regular file below a directory, in its subdirectories too, that has one of the given extensions.
	 *
	 * @param Root The directory to search.
	 * @param Extensions The extensions to accept, each with its dot: ".h".
	 * @param OutFiles Receives the files, sorted by path so that the result does not depend on the file system's
	 * order.
	 * @param OutError Receives the reason when the directory cannot be read.
	 * @return False if the directory, or one below it, cannot be read.
	 */
	static bool FindFiles(const std::filesystem::path& Root,
		std::span<const std::string_view> Extensions,
		std::vector<std::filesystem::path>& OutFiles,
		std::string& OutError);

	/**
	 * Reads a whole file, byte for byte.
	 *
	 * @return False if the file cannot be opened or read.
	 */
	static bool ReadTextFile(const std::filesystem::path& Path, std::string& OutText);

	/**
	 * Writes text to a file, creating its directory if needed, unless the file already holds exactly that text.
	 * Leaving an unchanged file alone keeps its modification time, so the build does not compile it again.
	 *
	 * @return False if the directory cannot be created or the file cannot be written.
	 */
	static bool WriteTextFileIfChanged(const std::filesystem::path& Path, std::string_view Text);
};
