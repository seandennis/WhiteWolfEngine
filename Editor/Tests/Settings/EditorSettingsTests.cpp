#include "Settings/EditorSettings.h"

#include "Core/FileHelper.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Platform/UserDirectories.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	/** Writes a file, which a test expects to work. */
	void WriteFile(const std::filesystem::path& Path, const std::string& Text)
	{
		std::string Error;
		const bool bSaved = FFileHelper::SaveStringToFile(Text, Path, Error);
		INFO("Error: ", Error);
		REQUIRE(bSaved);
	}

	/** Returns the project file of the game with the given number, in a directory of its own. */
	std::filesystem::path MakeProjectFile(const std::filesystem::path& Directory, int32 Number)
	{
		return Directory / std::format("Game{}", Number) / std::format("Game{}.wwproject", Number);
	}
}

TEST_SUITE("FEditorSettings")
{
	TEST_CASE("New settings remember no projects")
	{
		const FEditorSettings Settings;

		CHECK(Settings.GetRecentProjects().empty());
	}

	TEST_CASE("A project that is added comes before the ones that were added earlier")
	{
		const FScopedTempDirectory Directory;
		FEditorSettings Settings;

		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 1));
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 2));
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 3));

		const std::vector<std::filesystem::path> Expected = {MakeProjectFile(Directory.GetPath(), 3),
			MakeProjectFile(Directory.GetPath(), 2),
			MakeProjectFile(Directory.GetPath(), 1)};
		CHECK(Settings.GetRecentProjects() == Expected);
	}

	TEST_CASE("A project that is added again moves to the front, and is not there twice")
	{
		const FScopedTempDirectory Directory;
		FEditorSettings Settings;
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 1));
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 2));
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 3));
		std::filesystem::path AddedAgain = MakeProjectFile(Directory.GetPath(), 1);

		SUBCASE("By the same path")
		{
		}
		SUBCASE("By a path that goes somewhere else first")
		{
			AddedAgain = Directory.GetPath() / "Game2" / ".." / "Game1" / "." / "Game1.wwproject";
		}

		Settings.AddRecentProject(AddedAgain);

		// It is remembered by the path that leads straight to it.
		const std::vector<std::filesystem::path> Expected = {MakeProjectFile(Directory.GetPath(), 1),
			MakeProjectFile(Directory.GetPath(), 3),
			MakeProjectFile(Directory.GetPath(), 2)};
		CHECK(Settings.GetRecentProjects() == Expected);
	}

	TEST_CASE("A project that is reached by another path to the same file is there once")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Game" / "Game.wwproject";
		WriteFile(File, "{}");
#if defined(_WIN32)
		// The file system of Windows does not tell capital letters from small ones.
		const std::filesystem::path OtherPath = Directory.GetPath() / "GAME" / "GAME.WWPROJECT";
#else
		// A link to the project's directory is another way to the file.
		std::filesystem::create_directory_symlink(Directory.GetPath() / "Game", Directory.GetPath() / "Link");
		const std::filesystem::path OtherPath = Directory.GetPath() / "Link" / "Game.wwproject";
#endif
		FEditorSettings Settings;
		Settings.AddRecentProject(File);
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 2));

		Settings.AddRecentProject(OtherPath);

		// The path it was opened by last is the one that is kept.
		const std::vector<std::filesystem::path> Expected = {OtherPath, MakeProjectFile(Directory.GetPath(), 2)};
		CHECK(Settings.GetRecentProjects() == Expected);
	}

	TEST_CASE("Projects whose files are gone are told apart by their paths")
	{
		const FScopedTempDirectory Directory;
		FEditorSettings Settings;

		// Neither file exists, so the file system has nothing to say about them.
		Settings.AddRecentProject(Directory.GetPath() / "Game" / "Game.wwproject");
		Settings.AddRecentProject(Directory.GetPath() / "Other" / "Game.wwproject");

		CHECK(Settings.GetRecentProjects().size() == 2);
	}

	TEST_CASE("A relative path is remembered whole, from the current directory")
	{
		FEditorSettings Settings;

		Settings.AddRecentProject(std::filesystem::path("Games") / "Tetris" / "Tetris.wwproject");

		REQUIRE(Settings.GetRecentProjects().size() == 1);
		const std::filesystem::path& Remembered = Settings.GetRecentProjects().front();
		CHECK(Remembered.is_absolute());
		CHECK(Remembered == std::filesystem::current_path() / "Games" / "Tetris" / "Tetris.wwproject");
	}

	TEST_CASE("The project that was opened longest ago is forgotten when there are too many")
	{
		const FScopedTempDirectory Directory;
		FEditorSettings Settings;
		const int32 ProjectCount = FEditorSettings::MaxRecentProjectCount + 2;

		for (int32 Number = 1; Number <= ProjectCount; ++Number)
		{
			Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), Number));
		}

		const std::vector<std::filesystem::path>& Projects = Settings.GetRecentProjects();
		REQUIRE(Projects.size() == static_cast<std::size_t>(FEditorSettings::MaxRecentProjectCount));
		CHECK(Projects.front() == MakeProjectFile(Directory.GetPath(), ProjectCount));
		// The first two that were added are the ones that are gone.
		CHECK(Projects.back() == MakeProjectFile(Directory.GetPath(), 3));
	}

	TEST_CASE("A project without a file is a fatal error")
	{
		const FScopedLogCapture Capture;
		FEditorSettings Settings;

		CHECK_THROWS_AS(Settings.AddRecentProject(std::filesystem::path()), FFatalErrorException);
	}

	TEST_CASE("Settings that are saved are read back as they were")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		FEditorSettings Settings;
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 1));
		// A name with a space and with a letter outside ASCII, "Café", as UTF-8.
		Settings.AddRecentProject(Directory.GetPath() / FFileHelper::PathFromUtf8("My Caf\xC3\xA9") / "Game.wwproject");
		std::string Error;

		const bool bSaved = Settings.Save(File, Error);

		INFO("Error: ", Error);
		REQUIRE(bSaved);
		const std::optional<FEditorSettings> Loaded = FEditorSettings::Load(File, Error);
		INFO("Error: ", Error);
		REQUIRE(Loaded.has_value());
		CHECK(*Loaded == Settings);
		CHECK(Loaded->GetRecentProjects() == Settings.GetRecentProjects());
	}

	TEST_CASE("Settings without projects are saved and read back too")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		std::string Error;

		REQUIRE(FEditorSettings().Save(File, Error));
		const std::optional<FEditorSettings> Loaded = FEditorSettings::Load(File, Error);

		REQUIRE(Loaded.has_value());
		CHECK(Loaded->GetRecentProjects().empty());
	}

	TEST_CASE("A settings file says what it is, in which version, and names each project by its whole file")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		FEditorSettings Settings;
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 1));
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 2));
		std::string Error;
		REQUIRE(Settings.Save(File, Error));

		std::string Text;
		REQUIRE(FFileHelper::LoadFileToString(File, Text, Error));
		FJsonValue Document;
		REQUIRE(FJsonReader::Parse(Text, Document, Error));

		REQUIRE(Document.IsObject());
		REQUIRE(Document.FindMember("Format") != nullptr);
		CHECK(*Document.FindMember("Format") == FJsonValue("WhiteWolfEditorSettings"));
		REQUIRE(Document.FindMember("Version") != nullptr);
		CHECK(*Document.FindMember("Version") == FJsonValue(FEditorSettings::FormatVersion));
		const FJsonValue* const Projects = Document.FindMember("RecentProjects");
		REQUIRE(Projects != nullptr);
		REQUIRE(Projects->IsArray());
		REQUIRE(Projects->GetElements().size() == 2);
		CHECK(
			Projects->GetElements()[0] == FJsonValue(FFileHelper::PathToUtf8(MakeProjectFile(Directory.GetPath(), 2))));
		CHECK(
			Projects->GetElements()[1] == FJsonValue(FFileHelper::PathToUtf8(MakeProjectFile(Directory.GetPath(), 1))));
		// A file that people may look into ends with a line break.
		CHECK(Text.ends_with("}\n"));
	}

	TEST_CASE("Save creates the directory of the file, and replaces a file that is there")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "WhiteWolfEngine" / "Editor.json";
		FEditorSettings Settings;
		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 1));
		std::string Error;

		REQUIRE(Settings.Save(File, Error));
		CHECK(std::filesystem::is_regular_file(File));

		Settings.AddRecentProject(MakeProjectFile(Directory.GetPath(), 2));
		REQUIRE(Settings.Save(File, Error));

		const std::optional<FEditorSettings> Loaded = FEditorSettings::Load(File, Error);
		REQUIRE(Loaded.has_value());
		CHECK(Loaded->GetRecentProjects().size() == 2);
	}

	TEST_CASE("Save says why it cannot write the file")
	{
		const FScopedTempDirectory Directory;
		// Below a file there is no room for another.
		WriteFile(Directory.GetPath() / "Blocked", "A file, not a directory");
		const FEditorSettings Settings;
		std::string Error;

		CHECK_FALSE(Settings.Save(Directory.GetPath() / "Blocked" / "Editor.json", Error));

		CHECK_FALSE(Error.empty());
	}

	TEST_CASE("Load says why it cannot read settings from a file")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		std::string ExpectedReason;

		SUBCASE("A file that does not exist")
		{
			ExpectedReason = "cannot open";
		}
		SUBCASE("A file that is not JSON")
		{
			WriteFile(File, "{ not JSON");
			ExpectedReason = "is not a settings file: ";
		}
		SUBCASE("A file of another format")
		{
			WriteFile(File, R"({"Format": "WhiteWolfProject", "Version": 1, "Name": "Game"})");
			ExpectedReason = "is not a settings file: it lacks \"Format\": \"WhiteWolfEditorSettings\"";
		}
		SUBCASE("A file that is not an object")
		{
			WriteFile(File, "[1, 2, 3]");
			ExpectedReason = "is not a settings file: it lacks \"Format\": \"WhiteWolfEditorSettings\"";
		}
		SUBCASE("A file without a version")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings"})");
			ExpectedReason = "does not say which version of the settings format it is";
		}
		SUBCASE("A file whose version is not a whole number")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": "1"})");
			ExpectedReason = "does not say which version of the settings format it is";
		}
		SUBCASE("A file of a version before the first")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": 0})");
			ExpectedReason = "does not say which version of the settings format it is";
		}
		SUBCASE("A file of a later version")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": 2})");
			ExpectedReason = "is of version 2 of the settings format, and this editor reads up to version 1";
		}
		SUBCASE("Recent projects that are not a list")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": 1, "RecentProjects": "Game"})");
			ExpectedReason = "does not give the recent projects as a list of their files";
		}
		SUBCASE("A recent project that is not text")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": 1, "RecentProjects": ["A", 2]})");
			ExpectedReason = "does not give the recent projects as a list of their files";
		}
		SUBCASE("A recent project without a file")
		{
			WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": 1, "RecentProjects": ["A", ""]})");
			ExpectedReason = "does not give the recent projects as a list of their files";
		}

		std::string Error;
		CHECK_FALSE(FEditorSettings::Load(File, Error).has_value());

		INFO("Error: ", Error);
		CHECK(Error.find(ExpectedReason) != std::string::npos);
	}

	TEST_CASE("Settings that say nothing about projects are read as remembering none")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		WriteFile(File, R"({"Format": "WhiteWolfEditorSettings", "Version": 1})");
		std::string Error;

		const std::optional<FEditorSettings> Settings = FEditorSettings::Load(File, Error);

		INFO("Error: ", Error);
		REQUIRE(Settings.has_value());
		CHECK(Settings->GetRecentProjects().empty());
	}

	TEST_CASE("Of more projects than are remembered, the ones that were opened last are read")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		const int32 WrittenCount = FEditorSettings::MaxRecentProjectCount + 3;
		FJsonValue Projects = FJsonValue::MakeArray();
		for (int32 Number = 1; Number <= WrittenCount; ++Number)
		{
			Projects.AddElement(std::format("Game{}.wwproject", Number));
		}
		FJsonValue Document = FJsonValue::MakeObject();
		Document.SetMember("Format", "WhiteWolfEditorSettings");
		Document.SetMember("Version", FEditorSettings::FormatVersion);
		Document.SetMember("RecentProjects", std::move(Projects));
		WriteFile(File, FJsonWriter::Write(Document));
		std::string Error;

		const std::optional<FEditorSettings> Settings = FEditorSettings::Load(File, Error);

		INFO("Error: ", Error);
		REQUIRE(Settings.has_value());
		const std::vector<std::filesystem::path>& Read = Settings->GetRecentProjects();
		REQUIRE(Read.size() == static_cast<std::size_t>(FEditorSettings::MaxRecentProjectCount));
		// The files are taken as they are written, the first of them first.
		CHECK(Read.front() == std::filesystem::path("Game1.wwproject"));
		CHECK(Read.back() ==
			std::filesystem::path(std::format("Game{}.wwproject", FEditorSettings::MaxRecentProjectCount)));
	}

	TEST_CASE("The user's settings file is in a directory of the engine's own, below the user's settings directory")
	{
		const std::filesystem::path File = FEditorSettings::GetUserFile();
		const std::filesystem::path SettingsDirectory = FUserDirectories::GetSettingsDirectory();

		if (SettingsDirectory.empty())
		{
			// A system that does not say where settings go has no file for them.
			CHECK(File.empty());
		}
		else
		{
			CHECK(File == SettingsDirectory / "WhiteWolfEngine" / "Editor.json");
		}
	}
}
