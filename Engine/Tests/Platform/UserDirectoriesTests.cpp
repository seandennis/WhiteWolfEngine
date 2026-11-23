#include "Platform/UserDirectories.h"

#include <doctest/doctest.h>

#include <filesystem>

// What these directories are is the operating system's to say, and differs from one machine to the next. What can
// be tested everywhere is what a caller relies on: a whole path or none, and the same answer every time.

TEST_SUITE("FUserDirectories")
{
	TEST_CASE("The settings directory is a whole path, or none")
	{
		const std::filesystem::path Directory = FUserDirectories::GetSettingsDirectory();

		const bool bIsUsable = Directory.empty() || Directory.is_absolute();
		CHECK(bIsUsable);
		CHECK(FUserDirectories::GetSettingsDirectory() == Directory);
	}

	TEST_CASE("The documents directory is a whole path, or none")
	{
		const std::filesystem::path Directory = FUserDirectories::GetDocumentsDirectory();

		const bool bIsUsable = Directory.empty() || Directory.is_absolute();
		CHECK(bIsUsable);
		CHECK(FUserDirectories::GetDocumentsDirectory() == Directory);
	}

#if defined(_WIN32)
	TEST_CASE("On Windows the system always says where both directories are, and they are there")
	{
		const std::filesystem::path Settings = FUserDirectories::GetSettingsDirectory();
		const std::filesystem::path Documents = FUserDirectories::GetDocumentsDirectory();

		REQUIRE_FALSE(Settings.empty());
		REQUIRE_FALSE(Documents.empty());
		CHECK(std::filesystem::is_directory(Settings));
		CHECK(Settings != Documents);
	}
#endif
}
