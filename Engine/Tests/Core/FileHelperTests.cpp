#include "Core/FileHelper.h"

#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>

TEST_SUITE("FFileHelper")
{
	TEST_CASE("A saved string is loaded back byte for byte")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Text.txt";
		// Line endings of both kinds, UTF-8 and a zero byte: none of them may be translated or cut off.
		const std::string Original =
			std::string("first line\nsecond line\r\ncaf\xC3\xA9\n") + std::string(1, '\0') + "binary";
		std::string Error;
		std::string Loaded;

		REQUIRE(FFileHelper::SaveStringToFile(Original, Path, Error));
		REQUIRE(FFileHelper::LoadFileToString(Path, Loaded, Error));

		CHECK(Loaded == Original);
		CHECK(Error.empty());
	}

	TEST_CASE("An empty string makes an empty file")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Empty.txt";
		std::string Error;
		std::string Loaded = "not empty";

		REQUIRE(FFileHelper::SaveStringToFile("", Path, Error));
		REQUIRE(FFileHelper::LoadFileToString(Path, Loaded, Error));

		CHECK(Loaded.empty());
		CHECK(std::filesystem::file_size(Path) == 0);
	}

	TEST_CASE("Saving creates the directories of the path")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Saved" / "Scenes" / "Level.json";
		std::string Error;

		REQUIRE(FFileHelper::SaveStringToFile("{}", Path, Error));

		CHECK(std::filesystem::is_regular_file(Path));
	}

	TEST_CASE("Saving replaces an existing file and leaves no temporary file behind")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Text.txt";
		std::string Error;
		std::string Loaded;
		REQUIRE(FFileHelper::SaveStringToFile("a much longer first version", Path, Error));

		REQUIRE(FFileHelper::SaveStringToFile("second", Path, Error));
		REQUIRE(FFileHelper::LoadFileToString(Path, Loaded, Error));

		CHECK(Loaded == "second");
		int32 FileCount = 0;
		for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Directory.GetPath()))
		{
			INFO("File: ", Entry.path().string());
			++FileCount;
		}
		CHECK(FileCount == 1);
	}

	TEST_CASE("Loading a file that does not exist fails, names the file and leaves the output unchanged")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Missing.txt";
		std::string Error;
		std::string Loaded = "unchanged";

		CHECK_FALSE(FFileHelper::LoadFileToString(Path, Loaded, Error));

		CHECK(Loaded == "unchanged");
		CHECK(Error.find("Missing.txt") != std::string::npos);
	}

	TEST_CASE("Saving to a path that cannot be a file fails and names the path")
	{
		const FScopedTempDirectory Directory;
		// The target is an existing directory, which a file cannot replace.
		const std::filesystem::path Path = Directory.GetPath() / "Folder";
		std::filesystem::create_directory(Path);
		std::filesystem::create_directory(Path / "Inside");
		std::string Error;

		CHECK_FALSE(FFileHelper::SaveStringToFile("text", Path, Error));

		CHECK(Error.find("Folder") != std::string::npos);
		CHECK(std::filesystem::is_directory(Path));
		CHECK_FALSE(std::filesystem::exists(Directory.GetPath() / "Folder.tmp"));
	}

	TEST_CASE("PathToUtf8 returns the path as UTF-8 text")
	{
		const std::filesystem::path Path = std::filesystem::path(u8"Scènes") / "Level.json";

		const std::string Text = FFileHelper::PathToUtf8(Path);

		CHECK(Text.starts_with("Sc\xC3\xA8nes"));
		CHECK(Text.ends_with("Level.json"));
	}
}
