#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <ostream>
#include <string>

// Opening a scene is tested as a person does it: with Ctrl+O or the File menu, and then with the mouse on the list
// of the project's scenes or by typing the file.

namespace
{
	/**
	 * Saves a scene with one entity to a file of the editor's project and leaves the editor with an empty scene
	 * again, so that opening the file is seen to bring the entity back.
	 */
	void SaveSceneWithEntity(FEditorUiTest& Test, const std::string& File, const char* EntityName)
	{
		REQUIRE(Test.Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());
		FJsonValue CreateArguments = FJsonValue::MakeObject();
		CreateArguments.SetMember("Name", EntityName);
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", CreateArguments).HasSucceeded());
		FJsonValue SaveArguments = FJsonValue::MakeObject();
		SaveArguments.SetMember("Path", File);
		REQUIRE(Test.Editor.ExecuteCommand("Scene.Save", SaveArguments).HasSucceeded());
		REQUIRE(Test.Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());
		Test.DrawFrame();
	}
}

TEST_SUITE("FOpenSceneDialog")
{
	TEST_CASE("Ctrl+O lists the scenes of the project, and Open opens the one that was clicked")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		SaveSceneWithEntity(Test, "Town.wwscene", "Mayor");
		SaveSceneWithEntity(Test, "Scenes/Cave.wwscene", "Dragon");
		CHECK_FALSE(Test.HasItem("OpenScene/Path"));

		Test.PressKey(EKey::O, {EKey::LeftControl});
		REQUIRE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.HasItem("OpenScene/Town.wwscene"));
		REQUIRE(Test.HasItem("OpenScene/Scenes/Cave.wwscene"));

		// A click chooses, and changes nothing yet.
		Test.Click("OpenScene/Scenes/Cave.wwscene");
		CHECK(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());

		Test.Click("OpenScene/Open");

		CHECK_FALSE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Dragon") != nullptr);
		CHECK(Test.Editor.GetScenePath() == Content / "Scenes" / "Cave.wwscene");
		// The outliner shows what was opened.
		CHECK(Test.HasItem(std::format("Outliner/{}", Test.Editor.GetScene().FindEntityByName("Dragon")->GetId())));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A double click on a scene of the list opens it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		SaveSceneWithEntity(Test, "Town.wwscene", "Mayor");

		Test.PressKey(EKey::O, {EKey::LeftControl});
		REQUIRE(Test.HasItem("OpenScene/Town.wwscene"));
		const FVector2D Row = Test.RequireItem("OpenScene/Town.wwscene").GetCenter();
		Test.ClickAt(Row);
		Test.ClickAt(Row);

		CHECK_FALSE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Mayor") != nullptr);
	}

	TEST_CASE("The file can be typed, and Enter opens it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		SaveSceneWithEntity(Test, "Scenes/Cave.wwscene", "Dragon");

		Test.Click("Menu/File");
		Test.Click("Menu/File/Open Scene...");
		REQUIRE(Test.HasItem("OpenScene/Path"));
		// The field has the keyboard as the window opens.
		Test.Type("Scenes/Cave.wwscene");
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Dragon") != nullptr);
	}

	TEST_CASE("A file that cannot be opened leaves the window open and the scene as it was")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();

		Test.PressKey(EKey::O, {EKey::LeftControl});
		Test.Type("Nowhere.wwscene");
		Test.PressKey(EKey::Enter);

		CHECK(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);
		CHECK(Test.Editor.GetScenePath().empty());

		Test.Click("OpenScene/Cancel");
		CHECK_FALSE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("Escape closes the window, and so does Cancel, without opening anything")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		SaveSceneWithEntity(Test, "Town.wwscene", "Mayor");

		Test.PressKey(EKey::O, {EKey::LeftControl});
		REQUIRE(Test.HasItem("OpenScene/Town.wwscene"));
		Test.Click("OpenScene/Town.wwscene");
		Test.PressKey(EKey::Escape);

		CHECK_FALSE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK(Test.Editor.GetScenePath().empty());
	}

	TEST_CASE("Without a project there is no list, and the scene is opened by its whole path")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Lone.wwscene";
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Hermit"})")).HasSucceeded());
		FJsonValue SaveArguments = FJsonValue::MakeObject();
		SaveArguments.SetMember("Path", FFileHelper::PathToUtf8(File));
		REQUIRE(Test.Editor.ExecuteCommand("Scene.Save", SaveArguments).HasSucceeded());
		REQUIRE(Test.Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());
		Test.DrawFrame();

		Test.PressKey(EKey::O, {EKey::LeftControl});
		REQUIRE(Test.HasItem("OpenScene/Path"));
		Test.Type(FFileHelper::PathToUtf8(File));
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("OpenScene/Path"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Hermit") != nullptr);
		CHECK(Test.Editor.GetScenePath() == File);
	}

	TEST_CASE("The list is the project's scenes as they are when the window opens")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());

		Test.PressKey(EKey::O, {EKey::LeftControl});
		CHECK_FALSE(Test.HasItem("OpenScene/Town.wwscene"));
		Test.PressKey(EKey::Escape);

		SaveSceneWithEntity(Test, "Town.wwscene", "Mayor");
		Test.PressKey(EKey::O, {EKey::LeftControl});
		CHECK(Test.HasItem("OpenScene/Town.wwscene"));
	}
}
