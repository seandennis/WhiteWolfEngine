#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

// Saving a scene is tested as a person does it: with Ctrl+S, the File menu and the toolbar, and by typing into the
// window that asks where.

namespace
{
	/** Returns the text of a file, which a test expects to be there. */
	std::string ReadFile(const std::filesystem::path& Path)
	{
		std::string Text;
		std::string Error;
		const bool bLoaded = FFileHelper::LoadFileToString(Path, Text, Error);
		INFO("Error: ", Error);
		REQUIRE(bLoaded);
		return Text;
	}

	/** Adds an entity to the editor's scene with a command, as something the user did and can save. */
	void AddEntity(FEditorUiTest& Test, const char* Name)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Name", Name);
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", Arguments).HasSucceeded());
		Test.DrawFrame();
	}
}

TEST_SUITE("FSaveSceneDialog")
{
	TEST_CASE("Ctrl+S on a scene without a file asks where to save it, and Enter saves it there")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		AddEntity(Test, "Player");
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));

		Test.PressKey(EKey::S, {EKey::LeftControl});
		REQUIRE(Test.HasItem("SaveScene/Path"));
		// Nothing is saved until the user says where.
		CHECK(Test.Editor.GetScenePath().empty());

		// The field has the keyboard, with a name made of the scene's selected in it: what is typed replaces it.
		Test.Type("Town");
		Test.PressKey(EKey::Enter);

		// A name without the ending of scene files is given it.
		const std::filesystem::path File = Content / "Town.wwscene";
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(Test.Editor.GetScenePath() == File);
		REQUIRE(std::filesystem::is_regular_file(File));
		CHECK(ReadFile(File).find("\"Player\"") != std::string::npos);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Ctrl+S on a scene that has a file saves it there without asking")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		AddEntity(Test, "Player");
		REQUIRE(Test.Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Town.wwscene"})")).HasSucceeded());
		AddEntity(Test, "Dragon");
		REQUIRE(ReadFile(Content / "Town.wwscene").find("\"Dragon\"") == std::string::npos);

		Test.PressKey(EKey::S, {EKey::LeftControl});

		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(ReadFile(Content / "Town.wwscene").find("\"Dragon\"") != std::string::npos);
	}

	TEST_CASE("Save Scene As saves to another file, which becomes the scene's")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		AddEntity(Test, "Player");
		REQUIRE(Test.Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Town.wwscene"})")).HasSucceeded());

		Test.PressKey(EKey::S, {EKey::LeftControl, EKey::LeftShift});
		REQUIRE(Test.HasItem("SaveScene/Path"));
		// A folder of the content that is not there yet is made.
		Test.Type("Scenes/Village.wwscene");
		Test.Click("SaveScene/Save");

		const std::filesystem::path File = Content / "Scenes" / "Village.wwscene";
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(std::filesystem::is_regular_file(File));
		CHECK(Test.Editor.GetScenePath() == File);
		CHECK(std::filesystem::is_regular_file(Content / "Town.wwscene"));

		// From now on Ctrl+S means the new file.
		AddEntity(Test, "Dragon");
		Test.PressKey(EKey::S, {EKey::LeftControl});
		CHECK(ReadFile(File).find("\"Dragon\"") != std::string::npos);
		CHECK(ReadFile(Content / "Town.wwscene").find("\"Dragon\"") == std::string::npos);
	}

	TEST_CASE("Cancel and Escape close the window without saving anything")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());

		Test.PressKey(EKey::S, {EKey::LeftControl});
		REQUIRE(Test.HasItem("SaveScene/Cancel"));
		Test.Type("Town");
		SUBCASE("The button")
		{
			Test.Click("SaveScene/Cancel");
		}
		SUBCASE("The key")
		{
			Test.PressKey(EKey::Escape);
		}

		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(Test.Editor.GetScenePath().empty());
		CHECK(std::filesystem::is_empty(Content));
	}

	TEST_CASE(
		"A scene that cannot be saved where the user says leaves the window open, with the scene's file as it was")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		REQUIRE(Test.Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Town.wwscene"})")).HasSucceeded());

		// Below a file there is no room for another: "Town.wwscene" is a file, not a folder.
		Test.PressKey(EKey::S, {EKey::LeftControl, EKey::LeftShift});
		Test.Type("Town.wwscene/Square");
		Test.PressKey(EKey::Enter);

		CHECK(Test.HasItem("SaveScene/Path"));
		CHECK(Test.Editor.GetScenePath() == Content / "Town.wwscene");

		// The user can say another place in the same window.
		Test.Click("SaveScene/Path");
		Test.PressKey(EKey::A, {EKey::LeftControl});
		Test.Type("Square");
		Test.PressKey(EKey::Enter);
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(Test.Editor.GetScenePath() == Content / "Square.wwscene");
	}

	TEST_CASE("A scene that can no longer be saved to its file has the user asked for another")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		REQUIRE(Test.Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Town.wwscene"})")).HasSucceeded());
		// Where the scene's file was there is now a folder with something in it, which no file can replace.
		std::filesystem::remove(Content / "Town.wwscene");
		std::filesystem::create_directories(Content / "Town.wwscene" / "Inside");
		AddEntity(Test, "Player");

		Test.PressKey(EKey::S, {EKey::LeftControl});

		// The window opens instead of the scene looking saved, with the scene's file in its field.
		REQUIRE(Test.HasItem("SaveScene/Path"));
		CHECK(std::filesystem::is_directory(Content / "Town.wwscene"));

		Test.Type("Village");
		Test.PressKey(EKey::Enter);
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(Test.Editor.GetScenePath() == Content / "Village.wwscene");
		CHECK(ReadFile(Content / "Village.wwscene").find("\"Player\"") != std::string::npos);
	}

	TEST_CASE("Without a project, the scene is saved to the file whose whole path is typed")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		AddEntity(Test, "Player");

		Test.PressKey(EKey::S, {EKey::LeftControl});
		REQUIRE(Test.HasItem("SaveScene/Path"));
		Test.Type(FFileHelper::PathToUtf8(Directory.GetPath() / "Lone"));
		Test.PressKey(EKey::Enter);

		const std::filesystem::path File = Directory.GetPath() / "Lone.wwscene";
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(std::filesystem::is_regular_file(File));
		CHECK(Test.Editor.GetScenePath() == File);
	}

	TEST_CASE("The Save button of the toolbar and the entries of the File menu do what the shortcuts do")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		AddEntity(Test, "Player");

		// With no file yet, the button asks where.
		Test.Click("Toolbar/Save");
		REQUIRE(Test.HasItem("SaveScene/Path"));
		Test.Type("Town");
		Test.PressKey(EKey::Enter);
		REQUIRE(std::filesystem::is_regular_file(Content / "Town.wwscene"));

		// With one, it saves.
		AddEntity(Test, "Dragon");
		Test.Click("Toolbar/Save");
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));
		CHECK(ReadFile(Content / "Town.wwscene").find("\"Dragon\"") != std::string::npos);

		AddEntity(Test, "Knight");
		Test.Click("Menu/File");
		Test.Click("Menu/File/Save");
		CHECK(ReadFile(Content / "Town.wwscene").find("\"Knight\"") != std::string::npos);

		Test.Click("Menu/File");
		Test.Click("Menu/File/Save Scene As...");
		REQUIRE(Test.HasItem("SaveScene/Path"));
		Test.Type("Castle");
		Test.PressKey(EKey::Enter);
		CHECK(Test.Editor.GetScenePath() == Content / "Castle.wwscene");
	}

	TEST_CASE("While the window asks, the keys are its own and do nothing to the scene")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		AddEntity(Test, "Player");
		const int64 Player = Test.Editor.GetScene().FindEntityByName("Player")->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();

		Test.PressKey(EKey::S, {EKey::LeftControl});
		REQUIRE(Test.HasItem("SaveScene/Path"));
		Test.PressKey(EKey::Delete);
		Test.PressKey(EKey::N, {EKey::LeftControl});
		Test.PressKey(EKey::Escape);

		CHECK(Test.Editor.GetScene().FindEntityById(Player) != nullptr);
	}
}
