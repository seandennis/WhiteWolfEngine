#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Rendering/Image.h"
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

	/** Returns how many pixels of an item are as light as letters are: how much is written on it. */
	int32 CountInk(const FEditorUiTest& Test, const std::string& ItemName)
	{
		constexpr uint8 LeastInk = 150;
		const FUiRectangle Row = Test.RequireItem(ItemName);
		const FImage Picture = Test.ReadPicture();
		int32 Count = 0;
		for (int32 Y = static_cast<int32>(Row.Top); Y < static_cast<int32>(Row.Bottom); ++Y)
		{
			for (int32 X = static_cast<int32>(Row.Left); X < static_cast<int32>(Row.Right); ++X)
			{
				Count += Picture.GetPixel(X, Y).R > LeastInk ? 1 : 0;
			}
		}
		return Count;
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

	TEST_CASE("The name of a scene is shown whole, whatever is in it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		// Two number signs end the text of a row for the library that lays the list out, and are no more than
		// two letters of the name of a file.
		SaveSceneWithEntity(Test, "Level.wwscene", "Mayor");
		SaveSceneWithEntity(Test, "Level##One.wwscene", "Dragon");

		Test.PressKey(EKey::O, {EKey::LeftControl});
		REQUIRE(Test.HasItem("OpenScene/Level.wwscene"));
		REQUIRE(Test.HasItem("OpenScene/Level##One.wwscene"));

		// The longer name is written with more ink than the shorter one, which it would not be if it ended
		// after "Level".
		CHECK(CountInk(Test, "OpenScene/Level##One.wwscene") > CountInk(Test, "OpenScene/Level.wwscene"));

		// And it is the scene that a click on it chooses.
		Test.Click("OpenScene/Level##One.wwscene");
		Test.Click("OpenScene/Open");
		CHECK(Test.Editor.GetScene().FindEntityByName("Dragon") != nullptr);
	}

	TEST_CASE("A scene whose path is longer than the list is shown by its end")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		// Two scenes of one folder whose path alone is longer than the list is wide. What tells them apart is
		// their ends, one of wide letters and one of narrow ones.
		const std::string Folder =
			"Scenes/Chapters/TheLongWayRound/PartOne/Interiors/TheCellarsBelowTheOldTown/NorthWing/LowerLevel/";
		const std::string Wide = Folder + "MMMMMMMM.wwscene";
		const std::string Narrow = Folder + "iiiiiiii.wwscene";
		SaveSceneWithEntity(Test, Wide, "Mayor");
		SaveSceneWithEntity(Test, Narrow, "Dragon");

		Test.PressKey(EKey::O, {EKey::LeftControl});
		REQUIRE(Test.HasItem("OpenScene/" + Wide));
		REQUIRE(Test.HasItem("OpenScene/" + Narrow));

		// Shown from their beginnings, the two rows would be the same picture. Shown from their ends, the one
		// that ends in wide letters is written with more ink.
		CHECK(CountInk(Test, "OpenScene/" + Wide) > CountInk(Test, "OpenScene/" + Narrow));

		// The row is still the scene that a click on it chooses.
		Test.Click("OpenScene/" + Narrow);
		Test.Click("OpenScene/Open");
		CHECK(Test.Editor.GetScene().FindEntityByName("Dragon") != nullptr);
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
