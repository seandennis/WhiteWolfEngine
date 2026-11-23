#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Project/Project.h"
#include "Settings/EditorSettings.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
	/**
	 * Has an editor create a project called like its directory, and returns the project file.
	 */
	std::filesystem::path CreateProject(FEditor& Editor, const std::filesystem::path& Parent, const std::string& Name)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Directory", FFileHelper::PathToUtf8(Parent / Name));
		Arguments.SetMember("Name", Name);
		const FCommandResult Result = Editor.ExecuteCommand("Project.New", Arguments);
		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Parent / Name / (Name + ".wwproject");
	}

	/** Has an editor open a project, and returns whether it did. */
	bool OpenProject(FEditor& Editor, const std::filesystem::path& Path)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Path", FFileHelper::PathToUtf8(Path));
		return Editor.ExecuteCommand("Project.Open", Arguments).HasSucceeded();
	}

	/** Returns true if a warning that begins with the given words was logged. */
	bool WasWarned(const FScopedLogCapture& Capture, std::string_view Beginning)
	{
		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		return std::ranges::any_of(Messages,
			[Beginning](const FLogMessage& Message)
			{ return Message.Verbosity == ELogVerbosity::Warning && Message.Text.starts_with(Beginning); });
	}

	/** Returns the settings that a file holds, which a test expects to be a settings file. */
	FEditorSettings LoadSettings(const std::filesystem::path& File)
	{
		std::string Error;
		const std::optional<FEditorSettings> Settings = FEditorSettings::Load(File, Error);
		INFO("Error: ", Error);
		REQUIRE(Settings.has_value());
		return *Settings;
	}
}

TEST_SUITE("FEditor recent projects")
{
	TEST_CASE("A new editor remembers no projects")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		CHECK(Editor.GetSettings().GetRecentProjects().empty());
	}

	TEST_CASE("A project that is created or opened is the first of the recent projects")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;

		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");
		const std::filesystem::path Maze = CreateProject(Editor, Directory.GetPath(), "Maze");

		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Maze, Tetris});

		// Opening one that is remembered moves it to the front. By its directory, it is remembered by its file.
		REQUIRE(OpenProject(Editor, Directory.GetPath() / "Tetris"));

		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Tetris, Maze});
	}

	TEST_CASE("A project that could not be opened is not remembered")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");

		CHECK_FALSE(OpenProject(Editor, Directory.GetPath() / "Nowhere" / "Nowhere.wwproject"));

		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Tetris});
	}

	TEST_CASE("FindRecentProjects returns the recent projects that can be opened, the one opened last first")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		CHECK(Editor.FindRecentProjects().empty());

		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");
		const std::filesystem::path Maze = CreateProject(Editor, Directory.GetPath(), "Maze");

		const std::vector<FProject> Found = Editor.FindRecentProjects();
		REQUIRE(Found.size() == 2);
		CHECK(Found[0].GetName() == "Maze");
		CHECK(Found[0].GetFile() == Maze);
		CHECK(Found[1].GetName() == "Tetris");
		CHECK(Found[1].GetFile() == Tetris);
	}

	TEST_CASE("A recent project that cannot be opened now is not found, and is not forgotten either")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");
		const std::filesystem::path Maze = CreateProject(Editor, Directory.GetPath(), "Maze");

		SUBCASE("A project that was moved away")
		{
			std::filesystem::rename(Directory.GetPath() / "Tetris", Directory.GetPath() / "Elsewhere");

			const std::vector<FProject> Found = Editor.FindRecentProjects();
			REQUIRE(Found.size() == 1);
			CHECK(Found[0].GetFile() == Maze);

			// It is found again once it is back where it was.
			std::filesystem::rename(Directory.GetPath() / "Elsewhere", Directory.GetPath() / "Tetris");
			CHECK(Editor.FindRecentProjects().size() == 2);
		}
		SUBCASE("A file that is no longer a project file")
		{
			std::string Error;
			REQUIRE(FFileHelper::SaveStringToFile("{ not a project", Tetris, Error));

			const std::vector<FProject> Found = Editor.FindRecentProjects();
			REQUIRE(Found.size() == 1);
			CHECK(Found[0].GetFile() == Maze);
		}

		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Maze, Tetris});
		// That a project is gone is nothing to warn about: it is only not offered.
		CHECK(CountLoggedProblems(Capture) == 0);
	}

	TEST_CASE("An editor that is given no file leaves the settings of the person who is logged in alone")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path UserFile = FEditorSettings::GetUserFile();
		// Whether the file is there and when it was last written: neither may change. A file that is not there
		// has no time, which the error code says and the comparison below takes as it is.
		std::error_code ErrorBefore;
		const bool bExistedBefore = !UserFile.empty() && std::filesystem::exists(UserFile, ErrorBefore);
		const std::filesystem::file_time_type TimeBefore = std::filesystem::last_write_time(UserFile, ErrorBefore);

		{
			FEditor Editor;
			CreateProject(Editor, Directory.GetPath(), "Tetris");
		}

		std::error_code ErrorAfter;
		const bool bExistsAfter = !UserFile.empty() && std::filesystem::exists(UserFile, ErrorAfter);
		const std::filesystem::file_time_type TimeAfter = std::filesystem::last_write_time(UserFile, ErrorAfter);
		CHECK(bExistsAfter == bExistedBefore);
		CHECK(TimeAfter == TimeBefore);
	}

	TEST_CASE("An editor that is given a file writes the recent projects there when one is opened")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		const FScopedTempDirectory Directory;
		// The directory of the file is made with it.
		const std::filesystem::path File = Directory.GetPath() / "Settings" / "Editor.json";
		FEditor Editor;

		Editor.SetSettingsFile(File);

		// Nothing is written before there is something to remember.
		CHECK_FALSE(std::filesystem::exists(File));
		CHECK(Editor.GetSettings().GetRecentProjects().empty());

		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");

		REQUIRE(std::filesystem::is_regular_file(File));
		CHECK(LoadSettings(File).GetRecentProjects() == std::vector<std::filesystem::path>{Tetris});

		const std::filesystem::path Maze = CreateProject(Editor, Directory.GetPath(), "Maze");

		CHECK(LoadSettings(File).GetRecentProjects() == std::vector<std::filesystem::path>{Maze, Tetris});
		CHECK(LoadSettings(File) == Editor.GetSettings());
		CHECK(CountLoggedProblems(Capture) == 0);
	}

	TEST_CASE("An editor reads what an earlier one remembered")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		std::filesystem::path Tetris;
		{
			FEditor Earlier;
			Earlier.SetSettingsFile(File);
			Tetris = CreateProject(Earlier, Directory.GetPath(), "Tetris");
		}
		FEditor Editor;
		REQUIRE(Editor.GetSettings().GetRecentProjects().empty());

		Editor.SetSettingsFile(File);

		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Tetris});
		// Reading opens nothing.
		CHECK(Editor.GetProject() == nullptr);
	}

	TEST_CASE("What another editor wrote to the file in the meantime is kept")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		FEditor Editor;
		Editor.SetSettingsFile(File);
		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");

		// Another editor, run by the same person, opens a project of its own.
		const std::filesystem::path Chess = Directory.GetPath() / "Chess" / "Chess.wwproject";
		FEditorSettings Others = LoadSettings(File);
		Others.AddRecentProject(Chess);
		std::string Error;
		REQUIRE(Others.Save(File, Error));

		const std::filesystem::path Maze = CreateProject(Editor, Directory.GetPath(), "Maze");

		const std::vector<std::filesystem::path> Expected = {Maze, Chess, Tetris};
		CHECK(Editor.GetSettings().GetRecentProjects() == Expected);
		CHECK(LoadSettings(File).GetRecentProjects() == Expected);
	}

	TEST_CASE("A settings file that cannot be read is reported, and replaced when a project is opened")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile("{ not settings", File, Error));
		FEditor Editor;

		Editor.SetSettingsFile(File);

		CHECK(Editor.GetSettings().GetRecentProjects().empty());
		CHECK(WasWarned(Capture, "The editor's settings were not read: "));

		// What is in the file is told again before it is replaced.
		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");

		CHECK(LoadSettings(File).GetRecentProjects() == std::vector<std::filesystem::path>{Tetris});
		CHECK(CountLoggedProblems(Capture) == 2);
	}

	TEST_CASE("A settings file that cannot be written is reported, and the editor goes on with what it remembers")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		const FScopedTempDirectory Directory;
		std::string Error;
		// Below a file there is no room for another.
		REQUIRE(FFileHelper::SaveStringToFile("A file, not a directory", Directory.GetPath() / "Blocked", Error));
		FEditor Editor;
		Editor.SetSettingsFile(Directory.GetPath() / "Blocked" / "Editor.json");
		REQUIRE(CountLoggedProblems(Capture) == 0);

		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");

		CHECK(WasWarned(Capture, "The editor's settings were not written: "));
		CHECK(CountLoggedProblems(Capture) == 1);
		REQUIRE(Editor.GetProject() != nullptr);
		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Tetris});
	}

	TEST_CASE("An editor whose settings file is taken away writes there no longer")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Editor.json";
		FEditor Editor;
		Editor.SetSettingsFile(File);
		const std::filesystem::path Tetris = CreateProject(Editor, Directory.GetPath(), "Tetris");

		Editor.SetSettingsFile(std::filesystem::path());
		const std::filesystem::path Maze = CreateProject(Editor, Directory.GetPath(), "Maze");

		// The editor goes on remembering; the file stays as it was.
		CHECK(Editor.GetSettings().GetRecentProjects() == std::vector<std::filesystem::path>{Maze, Tetris});
		CHECK(LoadSettings(File).GetRecentProjects() == std::vector<std::filesystem::path>{Tetris});
	}
}
