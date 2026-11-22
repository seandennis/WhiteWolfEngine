#include "Project/Project.h"

#include "Core/FileHelper.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>

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
}

TEST_SUITE("FProject")
{
	TEST_CASE("Create makes the directory, the project file and the content directory")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path ProjectDirectory = Directory.GetPath() / "Games" / "Tetris";
		std::string Error;

		const std::optional<FProject> Project = FProject::Create(ProjectDirectory, "My Tetris", Error);

		INFO("Error: ", Error);
		REQUIRE(Project.has_value());
		CHECK(Project->GetName() == "My Tetris");
		CHECK(Project->GetFile() == ProjectDirectory / "My Tetris.wwproject");
		CHECK(Project->GetDirectory() == ProjectDirectory);
		CHECK(Project->GetContentDirectory() == ProjectDirectory / "Content");
		CHECK(std::filesystem::is_regular_file(Project->GetFile()));
		CHECK(std::filesystem::is_directory(Project->GetContentDirectory()));

		std::string Text;
		REQUIRE(FFileHelper::LoadFileToString(Project->GetFile(), Text, Error));
		CHECK(Text == "{\n  \"Format\": \"WhiteWolfProject\",\n  \"Version\": 1,\n  \"Name\": \"My Tetris\"\n}\n");
	}

	TEST_CASE("A project that was created can be opened by its file or by its directory")
	{
		const FScopedTempDirectory Directory;
		std::string Error;
		const std::optional<FProject> Created = FProject::Create(Directory.GetPath(), "Tetris", Error);
		REQUIRE(Created.has_value());
		std::filesystem::path Path;

		SUBCASE("By its file")
		{
			Path = Created->GetFile();
		}
		SUBCASE("By its directory")
		{
			Path = Directory.GetPath();
		}

		const std::optional<FProject> Opened = FProject::Open(Path, Error);

		INFO("Error: ", Error);
		REQUIRE(Opened.has_value());
		CHECK(Opened->GetName() == "Tetris");
		CHECK(Opened->GetFile() == Created->GetFile());
		CHECK(Opened->GetContentDirectory() == Directory.GetPath() / "Content");
	}

	TEST_CASE("Create refuses a name that could not name a file, and creates nothing")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path ProjectDirectory = Directory.GetPath() / "Game";
		std::string Name;

		SUBCASE("No name")
		{
			Name = "";
		}
		SUBCASE("Spaces only")
		{
			Name = "   ";
		}
		SUBCASE("A slash")
		{
			Name = "Games/Tetris";
		}
		SUBCASE("A dot")
		{
			Name = "Tetris.2";
		}
		SUBCASE("A colon")
		{
			Name = "Tetris: The Game";
		}

		std::string Error;
		CHECK_FALSE(FProject::Create(ProjectDirectory, Name, Error).has_value());

		CHECK(Error.find("cannot be the name of a project") != std::string::npos);
		CHECK_FALSE(std::filesystem::exists(ProjectDirectory));
	}

	TEST_CASE("Create refuses a directory that it cannot use")
	{
		const FScopedTempDirectory Directory;
		std::string Error;

		SUBCASE("A directory that holds a project already")
		{
			REQUIRE(FProject::Create(Directory.GetPath(), "First", Error).has_value());

			CHECK_FALSE(FProject::Create(Directory.GetPath(), "Second", Error).has_value());

			CHECK(Error.find("holds a project already") != std::string::npos);
			CHECK_FALSE(std::filesystem::exists(Directory.GetPath() / "Second.wwproject"));
		}
		SUBCASE("A file")
		{
			WriteFile(Directory.GetPath() / "Notes.txt", "Not a directory");

			CHECK_FALSE(FProject::Create(Directory.GetPath() / "Notes.txt", "Game", Error).has_value());

			CHECK(Error.find("is a file, not a directory") != std::string::npos);
		}
		SUBCASE("No directory")
		{
			CHECK_FALSE(FProject::Create(std::filesystem::path(), "Game", Error).has_value());

			CHECK(Error == "a project needs a directory");
		}
	}

	TEST_CASE("A directory with other files in it can become a project")
	{
		const FScopedTempDirectory Directory;
		WriteFile(Directory.GetPath() / "Readme.txt", "Already here");
		std::string Error;

		CHECK(FProject::Create(Directory.GetPath(), "Game", Error).has_value());
		CHECK(std::filesystem::exists(Directory.GetPath() / "Readme.txt"));
	}

	TEST_CASE("Open says why it cannot open a project")
	{
		const FScopedTempDirectory Directory;
		std::filesystem::path Path = Directory.GetPath() / "Game.wwproject";
		std::string ExpectedReason;

		SUBCASE("A file that does not exist")
		{
			ExpectedReason = "cannot open";
		}
		SUBCASE("A directory without a project file")
		{
			Path = Directory.GetPath();
			ExpectedReason = "holds no project file (.wwproject)";
		}
		SUBCASE("A directory with two project files")
		{
			WriteFile(
				Directory.GetPath() / "A.wwproject", R"({"Format": "WhiteWolfProject", "Version": 1, "Name": "A"})");
			WriteFile(
				Directory.GetPath() / "B.wwproject", R"({"Format": "WhiteWolfProject", "Version": 1, "Name": "B"})");
			Path = Directory.GetPath();
			ExpectedReason = "holds 2 project files; say which one";
		}
		SUBCASE("A file that is not JSON")
		{
			WriteFile(Path, "{ not JSON");
			ExpectedReason = "is not a project file: ";
		}
		SUBCASE("A file of another format")
		{
			WriteFile(Path, R"({"Format": "WhiteWolfScene", "Version": 1, "Name": "Game"})");
			ExpectedReason = "is not a project file: it lacks \"Format\": \"WhiteWolfProject\"";
		}
		SUBCASE("A file that is not an object")
		{
			WriteFile(Path, "[1, 2, 3]");
			ExpectedReason = "is not a project file: it lacks \"Format\": \"WhiteWolfProject\"";
		}
		SUBCASE("A file without a version")
		{
			WriteFile(Path, R"({"Format": "WhiteWolfProject", "Name": "Game"})");
			ExpectedReason = "does not say which version of the project format it is";
		}
		SUBCASE("A file of a later version")
		{
			WriteFile(Path, R"({"Format": "WhiteWolfProject", "Version": 2, "Name": "Game"})");
			ExpectedReason = "is of version 2 of the project format, and this editor reads up to version 1";
		}
		SUBCASE("A file without a name")
		{
			WriteFile(Path, R"({"Format": "WhiteWolfProject", "Version": 1})");
			ExpectedReason = "does not give the project a name";
		}
		SUBCASE("A file with an empty name")
		{
			WriteFile(Path, R"({"Format": "WhiteWolfProject", "Version": 1, "Name": ""})");
			ExpectedReason = "does not give the project a name";
		}

		std::string Error;
		CHECK_FALSE(FProject::Open(Path, Error).has_value());

		INFO("Error: ", Error);
		CHECK(Error.find(ExpectedReason) != std::string::npos);
	}

	TEST_CASE("The name of a project is what its file says, whatever the file is called")
	{
		const FScopedTempDirectory Directory;
		WriteFile(Directory.GetPath() / "Renamed.wwproject",
			R"({"Format": "WhiteWolfProject", "Version": 1, "Name": "The Real Name"})");
		std::string Error;

		const std::optional<FProject> Project = FProject::Open(Directory.GetPath(), Error);

		REQUIRE(Project.has_value());
		CHECK(Project->GetName() == "The Real Name");
	}
}
