#include "Core/FileHelper.h"
#include "Editor.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>

namespace
{
	/** Returns the arguments {"Path": Path}, with the path written as JSON requires. */
	FJsonValue MakePathArguments(const std::filesystem::path& Path)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Path", FFileHelper::PathToUtf8(Path));
		return Arguments;
	}

	/** Writes a scene file, which a test expects to work. */
	void WriteSceneFile(const std::filesystem::path& Path, const std::string& Text)
	{
		std::string Error;
		const bool bSaved = FFileHelper::SaveStringToFile(Text, Path, Error);
		INFO("File error: ", Error);
		REQUIRE(bSaved);
	}

	/** A scene file with an entity "Player" that has a child "Camera". */
	constexpr const char* TwoEntityScene = R"({
		"Format": "WhiteWolfScene",
		"Version": 1,
		"NextId": 3,
		"Properties": { "Name": "Level 1" },
		"Entities": [
			{ "Id": 1, "Properties": { "Name": "Player" }, "Components": [] },
			{ "Id": 2, "Parent": 1, "Properties": { "Name": "Camera" }, "Components": [] }
		]
	})";
}

TEST_SUITE("Scene commands")
{
	TEST_CASE("Scene.New replaces the scene with an empty one of the given name")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const TWeakObjectPtr<WScene> OldScene = &Editor.GetScene();
		REQUIRE(Editor.GetScene().CreateEntity("Player") != nullptr);
		Editor.SetScenePath(std::filesystem::path("Old.json"));

		const FCommandResult Result = Editor.ExecuteCommand("Scene.New", ParseTestJson(R"({"Name": "Level 2"})"));

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK(Editor.GetScene().GetName() == "Level 2");
		CHECK(Editor.GetScene().GetRootEntities().empty());
		CHECK(Editor.GetScenePath().empty());
		CHECK_FALSE(OldScene.IsValid());
	}

	TEST_CASE("Scene.New without a name calls the scene Untitled")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.GetScene().SetName("Level 1");

		REQUIRE(Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());

		CHECK(Editor.GetScene().GetName() == "Untitled");
	}

	TEST_CASE("Scene.Save writes the scene to the given file, which becomes the scene's file")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Scenes" / "Level.json";
		FEditor Editor;
		Editor.GetScene().SetName("Level 1");
		REQUIRE(Editor.GetScene().CreateEntity("Player") != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Save", MakePathArguments(Path));

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) == WriteTestJson(MakePathArguments(Path)));
		CHECK(Editor.GetScenePath() == Path);

		std::string Error;
		const WScene* const Loaded = FSceneSerializer::LoadFromFile(Path, Error);
		REQUIRE(Loaded != nullptr);
		CHECK(Loaded->GetName() == "Level 1");
		CHECK(Loaded->FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("Scene.Save without a path saves to the scene's file")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Level.json";
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Scene.Save", MakePathArguments(Path)).HasSucceeded());
		REQUIRE(Editor.GetScene().CreateEntity("Added later") != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Save", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) == WriteTestJson(MakePathArguments(Path)));
		std::string Error;
		const WScene* const Loaded = FSceneSerializer::LoadFromFile(Path, Error);
		REQUIRE(Loaded != nullptr);
		CHECK(Loaded->FindEntityByName("Added later") != nullptr);
	}

	TEST_CASE("Scene.Save fails for a scene that has no file when no path is given")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Save", FJsonValue());

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "The scene has no file yet. Give the argument \"Path\" to say where to save it.");
		CHECK(Editor.GetScenePath().empty());
	}

	TEST_CASE("Scene.Save that cannot write the file fails and leaves the scene's file as it was")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path GoodPath = Directory.GetPath() / "Level.json";
		// A file where a directory would have to be.
		const std::filesystem::path Blocker = Directory.GetPath() / "Blocker";
		WriteSceneFile(Blocker, "not a directory");
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Scene.Save", MakePathArguments(GoodPath)).HasSucceeded());

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Save", MakePathArguments(Blocker / "Level.json"));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError().starts_with("The scene was not saved: "));
		CHECK(Editor.GetScenePath() == GoodPath);
	}

	TEST_CASE("Scene.Open replaces the scene with the one in a file")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Level.json";
		WriteSceneFile(Path, TwoEntityScene);
		FEditor Editor;
		const TWeakObjectPtr<WScene> OldScene = &Editor.GetScene();

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Open", MakePathArguments(Path));

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) == R"({"Warnings":[]})");
		CHECK(Editor.GetScene().GetName() == "Level 1");
		CHECK(Editor.GetScenePath() == Path);
		CHECK_FALSE(OldScene.IsValid());

		const WEntity* const Camera = Editor.GetScene().FindEntityById(2);
		REQUIRE(Camera != nullptr);
		CHECK(Camera->GetName() == "Camera");
		REQUIRE(Camera->GetParent() != nullptr);
		CHECK(Camera->GetParent()->GetName() == "Player");

		// The loaded scene is the editor's: a collection leaves it alone.
		CollectGarbage();
		CHECK(Editor.GetScene().GetAllEntities().size() == 2);
	}

	TEST_CASE("Scene.Open returns the warnings about what could not be loaded")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Level.json";
		WriteSceneFile(Path,
			R"({
				"Format": "WhiteWolfScene",
				"Version": 1,
				"NextId": 2,
				"Properties": { "Name": "Level 1" },
				"Entities": [ { "Id": 1, "Properties": { "Name": "Player", "Mood": "Happy" }, "Components": [] } ]
			})");
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Open", MakePathArguments(Path));

		REQUIRE(Result.HasSucceeded());
		const FJsonValue* const Warnings = Result.GetValue().FindMember("Warnings");
		REQUIRE(Warnings != nullptr);
		REQUIRE(Warnings->IsArray());
		REQUIRE(Warnings->GetElements().size() == 1);
		REQUIRE(Warnings->GetElements()[0].IsString());
		CHECK(Warnings->GetElements()[0].GetString().find("Mood") != std::string::npos);
		CHECK(Editor.GetScene().FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("Scene.Open of a file that cannot be loaded fails and keeps the scene")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		Editor.GetScene().SetName("Kept");
		REQUIRE(Editor.GetScene().CreateEntity("Player") != nullptr);
		Editor.SetScenePath(std::filesystem::path("Kept.json"));
		std::filesystem::path Path;

		SUBCASE("A file that does not exist")
		{
			Path = Directory.GetPath() / "Missing.json";
		}
		SUBCASE("A file that is not JSON")
		{
			Path = Directory.GetPath() / "Broken.json";
			WriteSceneFile(Path, "{ \"Format\": ");
		}
		SUBCASE("A file that is not a scene")
		{
			Path = Directory.GetPath() / "Other.json";
			WriteSceneFile(Path, R"({"Format": "SomethingElse", "Version": 1})");
		}

		const FCommandResult Result = Editor.ExecuteCommand("Scene.Open", MakePathArguments(Path));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError().starts_with("The scene was not opened: "));
		CHECK(Editor.GetScene().GetName() == "Kept");
		CHECK(Editor.GetScene().FindEntityByName("Player") != nullptr);
		CHECK(Editor.GetScenePath() == std::filesystem::path("Kept.json"));
	}

	TEST_CASE("Scene.List returns the scene files of the project, in the order of their names")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		FJsonValue ProjectArguments = FJsonValue::MakeObject();
		ProjectArguments.SetMember("Directory", FFileHelper::PathToUtf8(Directory.GetPath() / "Game"));
		ProjectArguments.SetMember("Name", "Game");
		REQUIRE(Editor.ExecuteCommand("Project.New", ProjectArguments).HasSucceeded());

		// A project that has just been made has none.
		CHECK(WriteTestJson(Editor.ExecuteCommand("Scene.List", FJsonValue()).GetValue()) == "[]");

		REQUIRE(
			Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Scenes/Town.wwscene"})")).HasSucceeded());
		REQUIRE(Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Menu.wwscene"})")).HasSucceeded());
		REQUIRE(Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Scenes/Caves/Deep.wwscene"})"))
					.HasSucceeded());
		// A file of another name is a scene to whoever opens it, and not one that is listed.
		REQUIRE(Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Scenes/Old.json"})")).HasSucceeded());

		const FCommandResult Result = Editor.ExecuteCommand("Scene.List", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"(["Menu.wwscene","Scenes/Caves/Deep.wwscene","Scenes/Town.wwscene"])");

		// What is listed is what Scene.Open takes.
		CHECK(Editor.ExecuteCommand("Scene.Open", ParseTestJson(R"({"Path": "Scenes/Caves/Deep.wwscene"})"))
				  .HasSucceeded());
	}

	TEST_CASE("Scene.List fails while no project is open")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Scene.List", FJsonValue());

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			"No project is open, and it is the scenes of a project that are listed. Create one "
			"with Project.New or open one with Project.Open.");
	}

	TEST_CASE("Scene.GetHierarchy returns the tree of entities with their components")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WScene& Scene = Editor.GetScene();
		Scene.SetName("Level 1");
		WEntity* const Player = Scene.CreateEntity("Player");
		REQUIRE(Player != nullptr);
		WEntity* const Camera = Scene.CreateEntity("Camera", Player);
		REQUIRE(Camera != nullptr);
		REQUIRE(Scene.CreateEntity("Ground") != nullptr);
		REQUIRE(Player->AddComponent<WEditorTestComponent>() != nullptr);
		REQUIRE(Camera->AddComponent<WEditorTestDerivedComponent>() != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand("Scene.GetHierarchy", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		const FJsonValue Expected = ParseTestJson(R"({
			"Name": "Level 1",
			"Path": "",
			"Entities": [
				{
					"Id": 1,
					"Name": "Player",
					"Components": [ { "Id": 4, "Class": "WEditorTestComponent" } ],
					"Children": [
						{
							"Id": 2,
							"Name": "Camera",
							"Components": [ { "Id": 5, "Class": "WEditorTestDerivedComponent" } ],
							"Children": []
						}
					]
				},
				{ "Id": 3, "Name": "Ground", "Components": [], "Children": [] }
			]
		})");
		CHECK(WriteTestJson(Result.GetValue()) == WriteTestJson(Expected));
	}

	TEST_CASE("Scene.GetHierarchy of an empty scene has no entities, and gives the scene's file")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.SetScenePath(std::filesystem::path("Level.json"));

		const FCommandResult Result = Editor.ExecuteCommand("Scene.GetHierarchy", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) == R"({"Name":"Untitled","Path":"Level.json","Entities":[]})");
	}

	TEST_CASE("Scene.GetHierarchy shows a component whose class is missing as what it was saved as")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Level.json";
		WriteSceneFile(Path,
			R"({
				"Format": "WhiteWolfScene",
				"Version": 1,
				"NextId": 3,
				"Properties": { "Name": "Level 1" },
				"Entities": [
					{
						"Id": 1,
						"Properties": { "Name": "Crate" },
						"Components": [ { "Id": 2, "Class": "WPluginRigidBodyComponent", "Properties": { "Mass": 5 } } ]
					}
				]
			})");
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Scene.Open", MakePathArguments(Path)).HasSucceeded());

		const FCommandResult Result = Editor.ExecuteCommand("Scene.GetHierarchy", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		const FJsonValue* const Entities = Result.GetValue().FindMember("Entities");
		REQUIRE(Entities != nullptr);
		REQUIRE(Entities->GetElements().size() == 1);
		const FJsonValue* const Components = Entities->GetElements()[0].FindMember("Components");
		REQUIRE(Components != nullptr);
		CHECK(WriteTestJson(*Components) == R"([{"Id":2,"Class":"WPluginRigidBodyComponent","Missing":true}])");
	}
}
