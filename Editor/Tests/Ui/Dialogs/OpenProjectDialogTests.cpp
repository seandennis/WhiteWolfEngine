#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Project/Project.h"
#include "Scene/Scene.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>

// Opening a project is tested as a person does it: with the File menu, and then with the mouse on the list of the
// projects that were opened last or by typing where the project is.

namespace
{
	/** Returns the name that the row of a project has in the list of the window. */
	std::string RowOf(const std::filesystem::path& ProjectFile)
	{
		return std::format("OpenProject/{}", FFileHelper::PathToUtf8(ProjectFile));
	}

	/** Opens the window through the File menu. */
	void OpenWindow(FEditorUiTest& Test)
	{
		Test.Click("Menu/File");
		Test.Click("Menu/File/Open Project...");
		REQUIRE(Test.HasItem("OpenProject/Path"));
	}

	/** Returns the project file of the project that the editor works on, which a test expects it to have. */
	std::filesystem::path GetOpenProjectFile(const FEditor& Editor)
	{
		REQUIRE(Editor.GetProject() != nullptr);
		return Editor.GetProject()->GetFile();
	}
}

TEST_SUITE("FOpenProjectDialog")
{
	TEST_CASE("File > Open Project lists the projects that were opened last, and Open opens the one that was clicked")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath(), "Tetris");
		CreateTestProject(Test.Editor, Directory.GetPath(), "Maze");
		const std::filesystem::path Tetris = Directory.GetPath() / "Tetris" / "Tetris.wwproject";
		const std::filesystem::path Maze = Directory.GetPath() / "Maze" / "Maze.wwproject";
		Test.DrawFrame();
		CHECK_FALSE(Test.HasItem("OpenProject/Path"));

		OpenWindow(Test);
		REQUIRE(Test.HasItem(RowOf(Tetris)));
		REQUIRE(Test.HasItem(RowOf(Maze)));
		// The one that was opened last comes first.
		CHECK(Test.RequireItem(RowOf(Maze)).Top < Test.RequireItem(RowOf(Tetris)).Top);

		// A click chooses, and changes nothing yet.
		Test.Click(RowOf(Tetris));
		CHECK(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Maze);

		Test.Click("OpenProject/Open");

		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Tetris);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A double click on a project of the list opens it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath(), "Tetris");
		CreateTestProject(Test.Editor, Directory.GetPath(), "Maze");
		const std::filesystem::path Tetris = Directory.GetPath() / "Tetris" / "Tetris.wwproject";
		Test.DrawFrame();

		OpenWindow(Test);
		const FVector2D Row = Test.RequireItem(RowOf(Tetris)).GetCenter();
		Test.ClickAt(Row);
		Test.ClickAt(Row);

		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Tetris);
	}

	TEST_CASE("Where the project is can be typed, and Enter opens it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath(), "Tetris");
		CreateTestProject(Test.Editor, Directory.GetPath(), "Maze");
		const std::filesystem::path Tetris = Directory.GetPath() / "Tetris" / "Tetris.wwproject";
		Test.DrawFrame();
		std::filesystem::path TypedPath;

		SUBCASE("The project file")
		{
			TypedPath = Tetris;
		}
		SUBCASE("The directory of the project")
		{
			TypedPath = Directory.GetPath() / "Tetris";
		}

		OpenWindow(Test);
		// The field has the keyboard as the window opens.
		Test.Type(FFileHelper::PathToUtf8(TypedPath));
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Tetris);
	}

	TEST_CASE("A project that cannot be opened leaves the window open, and the editor with the project it had")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath(), "Tetris");
		const std::filesystem::path Tetris = Directory.GetPath() / "Tetris" / "Tetris.wwproject";
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();

		OpenWindow(Test);
		Test.Type(FFileHelper::PathToUtf8(Directory.GetPath() / "Nowhere"));
		Test.PressKey(EKey::Enter);

		CHECK(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Tetris);
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);

		// The user can say another place in the same window.
		Test.Click("OpenProject/Path");
		Test.PressKey(EKey::A, {EKey::LeftControl});
		Test.Type(FFileHelper::PathToUtf8(Tetris));
		Test.PressKey(EKey::Enter);
		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Tetris);
	}

	TEST_CASE("Cancel and Escape close the window without opening anything")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath(), "Tetris");
		CreateTestProject(Test.Editor, Directory.GetPath(), "Maze");
		const std::filesystem::path Tetris = Directory.GetPath() / "Tetris" / "Tetris.wwproject";
		const std::filesystem::path Maze = Directory.GetPath() / "Maze" / "Maze.wwproject";
		Test.DrawFrame();

		OpenWindow(Test);
		Test.Click(RowOf(Tetris));
		SUBCASE("The button")
		{
			Test.Click("OpenProject/Cancel");
		}
		SUBCASE("The key")
		{
			Test.PressKey(EKey::Escape);
		}

		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Maze);
	}

	TEST_CASE("The list is the recent projects that are there to be opened when the window opens")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath(), "Tetris");
		CreateTestProject(Test.Editor, Directory.GetPath(), "Maze");
		const std::filesystem::path Tetris = Directory.GetPath() / "Tetris" / "Tetris.wwproject";
		const std::filesystem::path Maze = Directory.GetPath() / "Maze" / "Maze.wwproject";
		Test.DrawFrame();

		// A project that was moved away since it was opened is not offered.
		std::filesystem::rename(Directory.GetPath() / "Tetris", Directory.GetPath() / "Elsewhere");
		OpenWindow(Test);
		CHECK_FALSE(Test.HasItem(RowOf(Tetris)));
		CHECK(Test.HasItem(RowOf(Maze)));
		Test.PressKey(EKey::Escape);

		// It is offered again once it is back.
		std::filesystem::rename(Directory.GetPath() / "Elsewhere", Directory.GetPath() / "Tetris");
		OpenWindow(Test);
		CHECK(Test.HasItem(RowOf(Tetris)));
		CHECK(Test.HasItem(RowOf(Maze)));
	}

	TEST_CASE("Before any project was opened the list is empty, and a project is opened by typing where it is")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		std::string Error;
		const std::optional<FProject> Tetris = FProject::Create(Directory.GetPath() / "Tetris", "Tetris", Error);
		REQUIRE(Tetris.has_value());
		REQUIRE(Test.Editor.GetSettings().GetRecentProjects().empty());

		OpenWindow(Test);
		CHECK_FALSE(Test.HasItem(RowOf(Tetris->GetFile())));
		Test.Type(FFileHelper::PathToUtf8(Tetris->GetFile()));
		Test.Click("OpenProject/Open");

		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(GetOpenProjectFile(Test.Editor) == Tetris->GetFile());

		// From now on it is one of the projects that were opened last.
		OpenWindow(Test);
		CHECK(Test.HasItem(RowOf(Tetris->GetFile())));
	}

	TEST_CASE("New Project closes the window and opens the one in which a project is made")
	{
		FEditorUiTest Test;

		OpenWindow(Test);
		REQUIRE(Test.HasItem("OpenProject/New"));
		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		Test.Click("OpenProject/New");

		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(Test.HasItem("NewProject/Name"));
		CHECK(Test.Editor.GetProject() == nullptr);

		// Nothing is made before the user says so.
		Test.PressKey(EKey::Escape);
		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(Test.Editor.GetProject() == nullptr);
	}

	TEST_CASE("AskForProject opens the window, as the editor does when it starts")
	{
		FEditorUiTest Test;
		REQUIRE_FALSE(Test.HasItem("OpenProject/Path"));

		Test.Ui->AskForProject();
		Test.DrawFrames(3);

		CHECK(Test.HasItem("OpenProject/Path"));
		CHECK(Test.HasItem("OpenProject/New"));

		// The user may also go on without a project.
		Test.Click("OpenProject/Cancel");
		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
		CHECK(Test.Editor.GetProject() == nullptr);
	}

	TEST_CASE("AskForProject does nothing while another window has to be answered")
	{
		FEditorUiTest Test;
		Test.PressKey(EKey::S, {EKey::LeftControl, EKey::LeftShift});
		REQUIRE(Test.HasItem("SaveScene/Path"));

		Test.Ui->AskForProject();
		Test.DrawFrames(3);

		CHECK(Test.HasItem("SaveScene/Path"));
		CHECK_FALSE(Test.HasItem("OpenProject/Path"));

		// What was asked then is not kept for when the other window is gone.
		Test.PressKey(EKey::Escape);
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK_FALSE(Test.HasItem("OpenProject/Path"));
	}
}
