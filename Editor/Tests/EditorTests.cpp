#include "Editor.h"

#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("FEditor")
{
	TEST_CASE("A new editor has an empty scene that is called Untitled and has no file")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		CHECK(Editor.GetScene().GetName() == "Untitled");
		CHECK(Editor.GetScene().GetRootEntities().empty());
		CHECK(Editor.GetScenePath().empty());
	}

	TEST_CASE("The editor keeps its scene alive")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;
		const TWeakObjectPtr<WScene> Scene = &Editor.GetScene();
		REQUIRE(Editor.GetScene().CreateEntity("Player") != nullptr);

		CollectGarbage();

		REQUIRE(Scene.IsValid());
		CHECK(&Editor.GetScene() == Scene.Get());
		CHECK(Editor.GetScene().GetAllEntities().size() == 1);
	}

	TEST_CASE("Destroying the editor destroys its scene")
	{
		const FScopedEditorTest EditorTest;
		TWeakObjectPtr<WScene> Scene;
		{
			const FEditor Editor;
			Scene = &Editor.GetScene();
			REQUIRE(Scene.IsValid());
		}

		CHECK_FALSE(Scene.IsValid());
	}

	TEST_CASE("The editor has the commands of the editor, scenes, entities, components and properties")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		std::vector<std::string> Names;
		for (const FCommandDescription* const Description : Editor.GetCommands().GetCommands())
		{
			Names.push_back(Description->Name);
			// Every command says what it does, and so does every parameter: this is what an agent reads.
			CAPTURE(Description->Name);
			CHECK_FALSE(Description->Summary.empty());
			for (const FCommandParameter& Parameter : Description->Parameters)
			{
				CAPTURE(Parameter.Name);
				CHECK_FALSE(Parameter.Description.empty());
			}
		}

		const std::vector<std::string> ExpectedNames = {
			"Component.Add",
			"Component.Remove",
			"Editor.ListCommands",
			"Entity.Create",
			"Entity.Delete",
			"Entity.Rename",
			"Entity.SetParent",
			"Object.GetProperties",
			"Object.GetProperty",
			"Object.SetProperty",
			"Scene.GetHierarchy",
			"Scene.New",
			"Scene.Open",
			"Scene.Save",
		};
		CHECK(Names == ExpectedNames);
	}

	TEST_CASE("SetScene replaces the scene and destroys the one it replaces")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const TWeakObjectPtr<WScene> OldScene = &Editor.GetScene();
		WEntity* const OldEntity = Editor.GetScene().CreateEntity("Old");
		REQUIRE(OldEntity != nullptr);
		const TWeakObjectPtr<WEntity> OldEntityReference = OldEntity;

		WScene* const NewScene = NewObject<WScene>();
		NewScene->SetName("Level 2");
		Editor.SetScene(*NewScene, std::filesystem::path("Levels") / "Level2.json");

		CHECK(&Editor.GetScene() == NewScene);
		CHECK(Editor.GetScenePath() == std::filesystem::path("Levels") / "Level2.json");
		CHECK_FALSE(OldScene.IsValid());
		CHECK_FALSE(OldEntityReference.IsValid());

		// The new scene is the editor's from now on: a collection leaves it alone.
		CollectGarbage();
		CHECK(IsValid(NewScene));
		CHECK(Editor.GetScene().GetName() == "Level 2");
	}

	TEST_CASE("SetScene with the scene the editor has already changes only its file")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WScene& Scene = Editor.GetScene();

		Editor.SetScene(Scene, std::filesystem::path("Saved.json"));

		CHECK(&Editor.GetScene() == &Scene);
		CHECK(IsValid(&Scene));
		CHECK(Editor.GetScenePath() == std::filesystem::path("Saved.json"));
	}

	TEST_CASE("SetScenePath records the file of the scene")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		Editor.SetScenePath(std::filesystem::path("Saved.json"));

		CHECK(Editor.GetScenePath() == std::filesystem::path("Saved.json"));
	}

	TEST_CASE("Giving the editor a destroyed scene is a fatal error")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		WScene* const Destroyed = NewObject<WScene>();
		Destroyed->Destroy();

		CHECK_THROWS_AS(Editor.SetScene(*Destroyed, std::filesystem::path()), FFatalErrorException);
		CHECK(Editor.GetScene().GetName() == "Untitled");
	}

	TEST_CASE("Destroying the editor's scene behind its back is a fatal error when the scene is next asked for")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		const FEditor Editor;

		Editor.GetScene().Destroy();

		CHECK_THROWS_AS(Editor.GetScene(), FFatalErrorException);
	}

	TEST_CASE("ExecuteCommand carries out a command on the editor")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})"));

		REQUIRE(Result.HasSucceeded());
		CHECK(Editor.GetScene().FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("A command that something else registered is carried out like the editor's own")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.GetCommands().Register({"Game.CountEntities", "Returns the number of entities in the scene.", {}},
			[](FEditor& CommandEditor, const FCommandArguments& /*Arguments*/)
			{ return FCommandResult::Success(static_cast<int64>(CommandEditor.GetScene().GetAllEntities().size())); });
		REQUIRE(Editor.GetScene().CreateEntity("One") != nullptr);
		REQUIRE(Editor.GetScene().CreateEntity("Two") != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand("Game.CountEntities", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue() == FJsonValue(2));
	}

	TEST_CASE("A command that fails is written to the log with its reason")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Entity.Delete", ParseTestJson(R"({"Entity": 99})"));

		CHECK_FALSE(Result.HasSucceeded());
		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		const bool bWasLogged = std::ranges::any_of(Messages,
			[](const FLogMessage& Message)
			{
				return Message.Category == "LogEditor" &&
					Message.Text == "Command Entity.Delete failed: There is no entity with the identifier 99.";
			});
		CHECK(bWasLogged);
	}
}
