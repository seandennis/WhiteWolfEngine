#include "Core/FileHelper.h"

#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ostream>
#include <string>
#include <system_error>

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

	TEST_CASE("DescribeError gives the words of the operating system without what it ends them with")
	{
		// The numbers of a few errors that every system has: no such file, no permission, the file exists.
		for (const int Code : {2, 5, 13, 17})
		{
			CAPTURE(Code);
			const std::error_code Error(Code, std::system_category());
			const std::string Words = FFileHelper::DescribeError(Error);

			REQUIRE_FALSE(Words.empty());
			CHECK(Words.back() != '.');
			CHECK(Words.back() != ' ');
			CHECK(Words.back() != '\n');
			CHECK(Words.back() != '\r');
			// Nothing but its end is taken away.
			CHECK(Error.message().starts_with(Words));
		}
	}

	TEST_CASE("The reason why a file cannot be saved goes into a sentence: it does not end in a full stop")
	{
		const FScopedTempDirectory Directory;
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile("text", Directory.GetPath() / "File", Error));

		// Below a file there is no room for another file.
		CHECK_FALSE(FFileHelper::SaveStringToFile("text", Directory.GetPath() / "File" / "Inside", Error));

		REQUIRE_FALSE(Error.empty());
		CHECK(Error.back() != '.');
		CHECK(Error.find("File") != std::string::npos);
	}

	TEST_CASE("IsSameNameIgnoringCase tells names apart by everything but the case of their letters")
	{
		CHECK(FFileHelper::IsSameNameIgnoringCase("Materials/Red.wwmaterial", "materials/RED.WWMATERIAL"));
		CHECK(FFileHelper::IsSameNameIgnoringCase("Red", "Red"));
		CHECK(FFileHelper::IsSameNameIgnoringCase("", ""));
		CHECK_FALSE(FFileHelper::IsSameNameIgnoringCase("Materials/Red.wwmaterial", "Materials/Rod.wwmaterial"));
		CHECK_FALSE(FFileHelper::IsSameNameIgnoringCase("Red", "Red "));
		CHECK_FALSE(FFileHelper::IsSameNameIgnoringCase("Red", "Reds"));
		CHECK_FALSE(FFileHelper::IsSameNameIgnoringCase("Red", ""));

		// Letters beyond A to Z have capitals too, as O with two dots: Windows does not tell those apart in the names
		// of files either, and elsewhere only A to Z are compared without their case.
		constexpr const char* CapitalOil = "\xC3\x96l";
		constexpr const char* SmallOil = "\xC3\xB6l";
#if defined(_WIN32)
		CHECK(FFileHelper::IsSameNameIgnoringCase(CapitalOil, SmallOil));
#else
		CHECK_FALSE(FFileHelper::IsSameNameIgnoringCase(CapitalOil, SmallOil));
#endif
	}
	TEST_CASE("PathToUtf8 returns the path as UTF-8 text")
	{
		const std::filesystem::path Path = std::filesystem::path(u8"Scènes") / "Level.json";

		const std::string Text = FFileHelper::PathToUtf8(Path);

		CHECK(Text.starts_with("Sc\xC3\xA8nes"));
		CHECK(Text.ends_with("Level.json"));
	}

	TEST_CASE("PathFromUtf8 returns the path that UTF-8 text names")
	{
		const std::filesystem::path Path = FFileHelper::PathFromUtf8("Sc\xC3\xA8nes/Level.json");

		CHECK(Path == std::filesystem::path(u8"Scènes") / "Level.json");
		CHECK(FFileHelper::PathFromUtf8("").empty());
	}

	TEST_CASE("A path survives the round trip through UTF-8 text")
	{
		const std::filesystem::path Path = std::filesystem::path(u8"Szenen") / u8"Übung 日本.json";

		CHECK(FFileHelper::PathFromUtf8(FFileHelper::PathToUtf8(Path)) == Path);
	}
}
