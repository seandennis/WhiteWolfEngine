#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Project/Project.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>

// Making a project is tested as a person does it: with the File menu, and by typing a name and a place into the
// window that asks for them.
//
// Every test that makes a project has opened one of its own first, in its temporary directory. The place that the
// window suggests is then that directory, and never the documents of whoever runs the tests.

namespace
{
	/** Opens the window through the File menu. */
	void OpenWindow(FEditorUiTest& Test)
	{
		Test.Click("Menu/File");
		Test.Click("Menu/File/New Project...");
		REQUIRE(Test.HasItem("NewProject/Name"));
	}

	/** Puts other text in the place of what a field of the window holds. */
	void TypeInto(FEditorUiTest& Test, std::string_view Field, std::string_view Text)
	{
		Test.Click(Field);
		Test.PressKey(EKey::A, {EKey::LeftControl});
		Test.Type(Text);
	}

	/** Empties a field of the window. */
	void Empty(FEditorUiTest& Test, std::string_view Field)
	{
		Test.Click(Field);
		Test.PressKey(EKey::A, {EKey::LeftControl});
		Test.PressKey(EKey::Backspace);
	}

	/** Returns how many files and directories a directory holds, not counting what is inside those. */
	std::ptrdiff_t CountEntries(const std::filesystem::path& Directory)
	{
		return std::distance(std::filesystem::directory_iterator(Directory), std::filesystem::directory_iterator());
	}

	/** Returns the project file of the project that the editor works on, which a test expects it to have. */
	std::filesystem::path GetOpenProjectFile(const FEditor& Editor)
	{
		REQUIRE(Editor.GetProject() != nullptr);
		return Editor.GetProject()->GetFile();
	}
}

TEST_SUITE("FNewProjectDialog")
{
	TEST_CASE("File > New Project asks for a name and a place, and Create makes the project there and opens it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		Test.DrawFrame();
		CHECK_FALSE(Test.HasItem("NewProject/Name"));

		OpenWindow(Test);
		// The field for the name has the keyboard, with a name in it that is typed over.
		Test.Type("Tetris");
		// A place that is not there yet is made.
		TypeInto(Test, "NewProject/Location", FFileHelper::PathToUtf8(Directory.GetPath() / "Games"));
		// Nothing is made before the user says so.
		CHECK_FALSE(std::filesystem::exists(Directory.GetPath() / "Games"));
		Test.Click("NewProject/Create");

		// The project's own directory is called like the project.
		const std::filesystem::path ProjectDirectory = Directory.GetPath() / "Games" / "Tetris";
		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == ProjectDirectory / "Tetris.wwproject");
		CHECK(Test.Editor.GetProject()->GetName() == "Tetris");
		CHECK(std::filesystem::is_regular_file(ProjectDirectory / "Tetris.wwproject"));
		CHECK(std::filesystem::is_directory(ProjectDirectory / "Content"));
		// It is now the project that was opened last.
		REQUIRE_FALSE(Test.Editor.GetSettings().GetRecentProjects().empty());
		CHECK(Test.Editor.GetSettings().GetRecentProjects().front() == ProjectDirectory / "Tetris.wwproject");
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The place that is suggested is beside the project that was opened last, and Enter makes the project")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath() / "Games", "First");
		Test.DrawFrame();

		OpenWindow(Test);
		Test.Type("Second");
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == Directory.GetPath() / "Games" / "Second" / "Second.wwproject");
	}

	TEST_CASE("Enter in the field for the place makes the project too")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		Test.DrawFrame();

		OpenWindow(Test);
		Test.Type("Tetris");
		TypeInto(Test, "NewProject/Location", FFileHelper::PathToUtf8(Directory.GetPath() / "Elsewhere"));
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == Directory.GetPath() / "Elsewhere" / "Tetris" / "Tetris.wwproject");
	}

	TEST_CASE("A project that cannot be made leaves the window open, and the editor with the project it had")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		const std::filesystem::path Game = Directory.GetPath() / "Game" / "Game.wwproject";
		Test.DrawFrame();

		OpenWindow(Test);
		SUBCASE("A name that cannot be a file's")
		{
			Test.Type("Tetris: The Game");
		}
		SUBCASE("A place that holds a project already")
		{
			// Beside the project called Game, a project of that name would be in the same directory.
			Test.Type("Game");
		}
		Test.PressKey(EKey::Enter);

		CHECK(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == Game);
		// Nothing was made: the project of before is all that the directory holds, and its file all that is its.
		CHECK(CountEntries(Directory.GetPath()) == 1);
		CHECK(CountEntries(Directory.GetPath() / "Game") == 2);

		// The user can put it right in the same window.
		TypeInto(Test, "NewProject/Name", "Tetris");
		Test.PressKey(EKey::Enter);
		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == Directory.GetPath() / "Tetris" / "Tetris.wwproject");
	}

	TEST_CASE("Create waits for a name and for a place")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		const std::filesystem::path Game = Directory.GetPath() / "Game" / "Game.wwproject";
		Test.DrawFrame();

		OpenWindow(Test);
		SUBCASE("Without a name")
		{
			Empty(Test, "NewProject/Name");
		}
		SUBCASE("Without a place")
		{
			Test.Type("Tetris");
			Empty(Test, "NewProject/Location");
		}
		// Neither the button nor the key does anything.
		Test.Click("NewProject/Create");
		Test.Click("NewProject/Name");
		Test.PressKey(EKey::Enter);

		CHECK(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == Game);
		CHECK(CountEntries(Directory.GetPath()) == 1);
	}

	TEST_CASE("Cancel and Escape close the window without making anything")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		const std::filesystem::path Game = Directory.GetPath() / "Game" / "Game.wwproject";
		Test.DrawFrame();

		OpenWindow(Test);
		Test.Type("Tetris");
		SUBCASE("The button")
		{
			Test.Click("NewProject/Cancel");
		}
		SUBCASE("The key")
		{
			Test.PressKey(EKey::Escape);
		}

		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(GetOpenProjectFile(Test.Editor) == Game);
		CHECK(CountEntries(Directory.GetPath()) == 1);
	}

	TEST_CASE("The window opens for a first project too, with a place suggested")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.GetSettings().GetRecentProjects().empty());

		// No project was opened that the new one could go beside. Nothing is made here: the place that is then
		// suggested is among the documents of whoever runs this test.
		OpenWindow(Test);

		CHECK(Test.HasItem("NewProject/Location"));
		CHECK(Test.HasItem("NewProject/Create"));
		Test.Click("NewProject/Cancel");
		CHECK_FALSE(Test.HasItem("NewProject/Name"));
		CHECK(Test.Editor.GetProject() == nullptr);
	}
}
