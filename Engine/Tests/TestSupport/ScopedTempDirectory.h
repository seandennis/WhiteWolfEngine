#pragma once

#include <filesystem>
#include <format>
#include <random>
#include <system_error>

/**
 * Creates a uniquely named, empty directory under the system's temporary directory and deletes it, with everything
 * in it, when destroyed. Gives each test its own place to write files.
 */
class FScopedTempDirectory
{
public:
	FScopedTempDirectory()
	{
		std::random_device RandomDevice;
		const std::filesystem::path Parent = std::filesystem::temp_directory_path();

		// Retry in the unlikely case that a directory with the chosen name already exists.
		bool bCreated = false;
		while (!bCreated)
		{
			Path = Parent / std::format("WhiteWolfTest-{:08x}{:08x}", RandomDevice(), RandomDevice());
			bCreated = std::filesystem::create_directory(Path);
		}
	}

	~FScopedTempDirectory()
	{
		// Cleaning up must not throw from a destructor; a leftover directory in the temp folder is harmless.
		std::error_code IgnoredError;
		std::filesystem::remove_all(Path, IgnoredError);
	}

	FScopedTempDirectory(const FScopedTempDirectory&) = delete;
	FScopedTempDirectory& operator=(const FScopedTempDirectory&) = delete;

	/** Returns the directory's path. */
	const std::filesystem::path& GetPath() const
	{
		return Path;
	}

private:
	std::filesystem::path Path;
};
