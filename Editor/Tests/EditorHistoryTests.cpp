#include "Core/FileHelper.h"
#include "Editor.h"
#include "History/UndoHistory.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using FNames = std::vector<std::string>;

	/** Gives the editor a command with the arguments written as JSON text, which a test expects to succeed. */
	FJsonValue Run(FEditor& Editor, std::string_view CommandName, std::string_view ArgumentText = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
		INFO("Command error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result.GetValue();
	}

	/** Returns the scene as the text of a scene file: everything about it that undo and redo have to bring back. */
	std::string WriteScene(const FEditor& Editor)
	{
		return FSceneSerializer::SaveToText(Editor.GetScene());
	}

	/**
	 * Builds a scene with what a history has to bring back: a hierarchy, components, property values of several
	 * kinds and a reference from a component to an entity.
	 */
	void BuildScene(FEditor& Editor)
	{
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Entity.Create", R"({"Name": "Camera", "Parent": 1})");
		Run(Editor, "Entity.Create", R"({"Name": "Target"})");
		Run(Editor, "Component.Add", R"({"Entity": 1, "Class": "WEditorTestDerivedComponent"})");
		Run(Editor, "Component.Add", R"({"Entity": 2, "Class": "WEditorTestComponent"})");
		Run(Editor, "Object.SetProperty", R"({"Object": 4, "Property": "Health", "Value": 80})");
		Run(Editor, "Object.SetProperty", R"({"Object": 4, "Property": "TargetEntity", "Value": 3})");
		Run(Editor, "Object.SetProperty", R"({"Object": 4, "Property": "Scores", "Value": [3, 5, 8]})");
		Run(Editor, "Object.SetProperty", R"({"Object": 4, "Property": "Range", "Value": 0.1})");
	}
}

TEST_SUITE("FEditor undo and redo")
{
	TEST_CASE("A new editor has nothing to undo or redo")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		CHECK_FALSE(Editor.GetHistory().CanUndo());
		CHECK_FALSE(Editor.GetHistory().CanRedo());
		CHECK_FALSE(Editor.Undo().has_value());
		CHECK_FALSE(Editor.Redo().has_value());
		CHECK(Editor.GetScene().GetName() == "Untitled");
	}

	TEST_CASE("A command that changes the scene is a step that can be undone and redone")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});

		const std::optional<std::string> Undone = Editor.Undo();

		REQUIRE(Undone.has_value());
		CHECK(*Undone == "Entity.Create");
		CHECK(Editor.GetScene().GetAllEntities().empty());
		CHECK(Editor.GetHistory().GetRedoStepNames() == FNames{"Entity.Create"});

		const std::optional<std::string> Redone = Editor.Redo();

		REQUIRE(Redone.has_value());
		CHECK(*Redone == "Entity.Create");
		const WEntity* const Player = Editor.GetScene().FindEntityById(1);
		REQUIRE(Player != nullptr);
		CHECK(Player->GetName() == "Player");
		CHECK_FALSE(Editor.GetHistory().CanRedo());
	}

	TEST_CASE("A command that changes nothing is not a step")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");

		Run(Editor, "Scene.GetHierarchy");
		Run(Editor, "Editor.ListCommands");
		Run(Editor, "Edit.GetHistory");
		Run(Editor, "Object.GetProperties", R"({"Object": 1})");
		// Renaming an entity to the name it has changes nothing either.
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Player"})");

		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("A command that fails is not a step")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");

		CHECK_FALSE(Editor.ExecuteCommand("Entity.Delete", ParseTestJson(R"({"Entity": 99})")).HasSucceeded());
		CHECK_FALSE(Editor.ExecuteCommand("No.SuchCommand", FJsonValue()).HasSucceeded());
		CHECK_FALSE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Nmae": "Typo"})")).HasSucceeded());

		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("Saving the scene is not a step")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Path", FFileHelper::PathToUtf8(Directory.GetPath() / "Level.json"));

		REQUIRE(Editor.ExecuteCommand("Scene.Save", Arguments).HasSucceeded());

		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("Undo brings back everything a command took away, as it was")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		BuildScene(Editor);
		const std::string Before = WriteScene(Editor);
		std::string CommandName;
		std::string ArgumentText;

		SUBCASE("An entity with its child and their components")
		{
			CommandName = "Entity.Delete";
			ArgumentText = R"({"Entity": 1})";
		}
		SUBCASE("An entity that a component refers to")
		{
			CommandName = "Entity.Delete";
			ArgumentText = R"({"Entity": 3})";
		}
		SUBCASE("A component")
		{
			CommandName = "Component.Remove";
			ArgumentText = R"({"Component": 4})";
		}
		SUBCASE("The value of a property")
		{
			CommandName = "Object.SetProperty";
			ArgumentText = R"({"Object": 4, "Property": "Scores", "Value": []})";
		}
		SUBCASE("A reference")
		{
			CommandName = "Object.SetProperty";
			ArgumentText = R"({"Object": 4, "Property": "TargetEntity", "Value": null})";
		}
		SUBCASE("A name")
		{
			CommandName = "Entity.Rename";
			ArgumentText = R"({"Entity": 2, "Name": "Eye"})";
		}
		SUBCASE("A place in the hierarchy")
		{
			CommandName = "Entity.SetParent";
			ArgumentText = R"({"Entity": 2, "Parent": 3})";
		}
		SUBCASE("A transform")
		{
			// Ten billion kilometres from the origin and a quarter of a metre: a location has to come back exactly.
			FJsonValue Transform =
				Run(Editor, "Object.GetProperty", R"({"Object": 1, "Property": "RelativeTransform"})");
			Transform.SetMember("Location", ParseTestJson("[10000000000000.0, 0.25, -3.0]"));
			CommandName = "Object.SetProperty";
			ArgumentText = std::format(
				R"({{"Object": 1, "Property": "RelativeTransform", "Value": {}}})", WriteTestJson(Transform));
		}

		Run(Editor, CommandName, ArgumentText);
		const std::string After = WriteScene(Editor);
		REQUIRE(After != Before);

		const std::optional<std::string> Undone = Editor.Undo();

		REQUIRE(Undone.has_value());
		CHECK(*Undone == CommandName);
		CHECK(WriteScene(Editor) == Before);

		const std::optional<std::string> Redone = Editor.Redo();

		REQUIRE(Redone.has_value());
		CHECK(*Redone == CommandName);
		CHECK(WriteScene(Editor) == After);
	}

	TEST_CASE("Steps are undone last first and redone in the order they were made")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		std::vector<std::string> States = {WriteScene(Editor)};
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		States.push_back(WriteScene(Editor));
		Run(Editor, "Component.Add", R"({"Entity": 1, "Class": "WEditorTestComponent"})");
		States.push_back(WriteScene(Editor));
		Run(Editor, "Object.SetProperty", R"({"Object": 2, "Property": "Label", "Value": "Hero"})");
		States.push_back(WriteScene(Editor));
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Object.SetProperty", "Component.Add", "Entity.Create"});

		REQUIRE(Editor.Undo() == std::optional<std::string>("Object.SetProperty"));
		CHECK(WriteScene(Editor) == States[2]);
		REQUIRE(Editor.Undo() == std::optional<std::string>("Component.Add"));
		CHECK(WriteScene(Editor) == States[1]);
		REQUIRE(Editor.Undo() == std::optional<std::string>("Entity.Create"));
		CHECK(WriteScene(Editor) == States[0]);
		CHECK_FALSE(Editor.Undo().has_value());
		CHECK(Editor.GetHistory().GetRedoStepNames() == FNames{"Entity.Create", "Component.Add", "Object.SetProperty"});

		REQUIRE(Editor.Redo() == std::optional<std::string>("Entity.Create"));
		CHECK(WriteScene(Editor) == States[1]);
		REQUIRE(Editor.Redo() == std::optional<std::string>("Component.Add"));
		CHECK(WriteScene(Editor) == States[2]);
		REQUIRE(Editor.Redo() == std::optional<std::string>("Object.SetProperty"));
		CHECK(WriteScene(Editor) == States[3]);
		CHECK_FALSE(Editor.Redo().has_value());
	}

	TEST_CASE("A change after an undo takes the place of what could have been redone")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Hero"})");
		REQUIRE(Editor.Undo().has_value());

		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Villain"})");

		CHECK_FALSE(Editor.Redo().has_value());
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Rename", "Entity.Create"});
		REQUIRE(Editor.Undo().has_value());
		const WEntity* const Entity = Editor.GetScene().FindEntityById(1);
		REQUIRE(Entity != nullptr);
		CHECK(Entity->GetName() == "Player");
	}

	TEST_CASE("Undo and redo replace the scene object, and the editor keeps the new one alive")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Hero"})");
		const TWeakObjectPtr<WScene> SceneBefore = &Editor.GetScene();
		const TWeakObjectPtr<WEntity> EntityBefore = Editor.GetScene().FindEntityById(1);
		REQUIRE(EntityBefore.IsValid());

		REQUIRE(Editor.Undo().has_value());

		// The old objects are gone at once; the entity is found again by its identifier.
		CHECK_FALSE(SceneBefore.IsValid());
		CHECK_FALSE(EntityBefore.IsValid());
		CollectGarbage();
		const WEntity* const Entity = Editor.GetScene().FindEntityById(1);
		REQUIRE(Entity != nullptr);
		CHECK(Entity->GetName() == "Player");

		REQUIRE(Editor.Redo().has_value());
		CollectGarbage();
		const WEntity* const EntityAgain = Editor.GetScene().FindEntityById(1);
		REQUIRE(EntityAgain != nullptr);
		CHECK(EntityAgain->GetName() == "Hero");
	}

	TEST_CASE("An undone entity's identifier is given to the next entity, as if the undone one had never been")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		CHECK(WriteTestJson(Run(Editor, "Entity.Create", R"({"Name": "Mistake"})")) == R"({"Id":2})");
		REQUIRE(Editor.Undo().has_value());

		CHECK(WriteTestJson(Run(Editor, "Entity.Create", R"({"Name": "Camera"})")) == R"({"Id":2})");
	}

	TEST_CASE("A change made to the scene without a command is a step of its own")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		WEntity* const Player = Editor.GetScene().FindEntityById(1);
		REQUIRE(Player != nullptr);
		Player->SetName("Renamed directly");

		SUBCASE("Undo undoes it, and nothing else")
		{
			const std::optional<std::string> Undone = Editor.Undo();

			REQUIRE(Undone.has_value());
			CHECK(*Undone == FEditor::DirectChangeStepName);
			const WEntity* const Restored = Editor.GetScene().FindEntityById(1);
			REQUIRE(Restored != nullptr);
			CHECK(Restored->GetName() == "Player");
			CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
			CHECK(Editor.GetHistory().GetRedoStepNames() == FNames{"Direct change"});
		}
		SUBCASE("The next command that changes the scene takes it into its step")
		{
			Run(Editor, "Entity.Create", R"({"Name": "Camera"})");
			CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create", "Entity.Create"});

			REQUIRE(Editor.Undo().has_value());

			CHECK(Editor.GetScene().FindEntityById(2) == nullptr);
			const WEntity* const Restored = Editor.GetScene().FindEntityById(1);
			REQUIRE(Restored != nullptr);
			CHECK(Restored->GetName() == "Player");
		}
		SUBCASE("It takes the place of what could have been redone")
		{
			// Back to before the direct change, then to before the entity, and then a change by hand.
			REQUIRE(Editor.Undo().has_value());
			REQUIRE(Editor.Undo().has_value());
			REQUIRE(Editor.GetHistory().CanRedo());
			Editor.GetScene().SetName("Renamed directly");

			CHECK_FALSE(Editor.Redo().has_value());
			CHECK(Editor.GetScene().GetName() == "Renamed directly");
			CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Direct change"});
		}
	}

	TEST_CASE("A change by a command that says it leaves the scene as it is is recorded as a change without a command")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.GetCommands().Register({"Test.RenameQuietly",
										  "Renames the scene, although it says it changes nothing.",
										  {},
										  ECommandEffect::LeavesScene},
			[](FEditor& CommandEditor, const FCommandArguments& /*Arguments*/)
			{
				CommandEditor.GetScene().SetName("Renamed quietly");
				return FCommandResult::Success();
			});
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");

		Run(Editor, "Test.RenameQuietly");

		// The editor did not look at the scene after the command, so there is no step yet.
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});

		const std::optional<std::string> Undone = Editor.Undo();

		REQUIRE(Undone.has_value());
		CHECK(*Undone == FEditor::DirectChangeStepName);
		CHECK(Editor.GetScene().GetName() == "Untitled");
		CHECK(Editor.GetScene().FindEntityById(1) != nullptr);
	}

	TEST_CASE("Another scene starts the history anew")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		FJsonValue PathArguments = FJsonValue::MakeObject();
		PathArguments.SetMember("Path", FFileHelper::PathToUtf8(Directory.GetPath() / "Level.json"));
		REQUIRE(Editor.ExecuteCommand("Scene.Save", PathArguments).HasSucceeded());
		Run(Editor, "Entity.Create", R"({"Name": "Camera"})");
		REQUIRE(Editor.Undo().has_value());
		REQUIRE(Editor.GetHistory().CanUndo());
		REQUIRE(Editor.GetHistory().CanRedo());

		SUBCASE("Scene.New")
		{
			Run(Editor, "Scene.New", R"({"Name": "Level 2"})");
		}
		SUBCASE("Scene.Open")
		{
			REQUIRE(Editor.ExecuteCommand("Scene.Open", PathArguments).HasSucceeded());
		}
		SUBCASE("SetScene")
		{
			Editor.SetScene(*NewObject<WScene>(), std::filesystem::path());
		}

		CHECK_FALSE(Editor.GetHistory().CanUndo());
		CHECK_FALSE(Editor.GetHistory().CanRedo());
		const std::string Scene = WriteScene(Editor);
		CHECK_FALSE(Editor.Undo().has_value());
		CHECK(WriteScene(Editor) == Scene);

		// The new scene has a history of its own.
		Run(Editor, "Entity.Create", R"({"Name": "First of the new scene"})");
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("Undo and redo leave the scene's file as it is")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.SetScenePath(std::filesystem::path("Level.json"));
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");

		REQUIRE(Editor.Undo().has_value());
		CHECK(Editor.GetScenePath() == std::filesystem::path("Level.json"));
		REQUIRE(Editor.Redo().has_value());
		CHECK(Editor.GetScenePath() == std::filesystem::path("Level.json"));
	}

	TEST_CASE("A component whose class is missing survives undo and redo with its data")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Level.json";
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile(
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
			})",
			Path,
			Error));
		FEditor Editor;
		FJsonValue PathArguments = FJsonValue::MakeObject();
		PathArguments.SetMember("Path", FFileHelper::PathToUtf8(Path));
		REQUIRE(Editor.ExecuteCommand("Scene.Open", PathArguments).HasSucceeded());
		const std::string Before = WriteScene(Editor);
		REQUIRE(Before.find("Mass") != std::string::npos);

		Run(Editor, "Entity.Delete", R"({"Entity": 1})");
		REQUIRE(Editor.Undo().has_value());

		CHECK(WriteScene(Editor) == Before);
	}
}

TEST_SUITE("Edit commands")
{
	TEST_CASE("Edit.Undo undoes the last change and returns the name of its command")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Hero"})");

		CHECK(WriteTestJson(Run(Editor, "Edit.Undo")) == R"({"Undone":"Entity.Rename"})");

		const WEntity* const Entity = Editor.GetScene().FindEntityById(1);
		REQUIRE(Entity != nullptr);
		CHECK(Entity->GetName() == "Player");
	}

	TEST_CASE("Edit.Redo redoes the change undone last and returns the name of its command")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Hero"})");
		Run(Editor, "Edit.Undo");

		CHECK(WriteTestJson(Run(Editor, "Edit.Redo")) == R"({"Redone":"Entity.Rename"})");

		const WEntity* const Entity = Editor.GetScene().FindEntityById(1);
		REQUIRE(Entity != nullptr);
		CHECK(Entity->GetName() == "Hero");
	}

	TEST_CASE("Edit.Undo and Edit.Redo fail when there is nothing to undo or redo")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Undone = Editor.ExecuteCommand("Edit.Undo", FJsonValue());
		CHECK_FALSE(Undone.HasSucceeded());
		CHECK(Undone.GetError() == "There is nothing to undo.");

		const FCommandResult Redone = Editor.ExecuteCommand("Edit.Redo", FJsonValue());
		CHECK_FALSE(Redone.HasSucceeded());
		CHECK(Redone.GetError() == "There is nothing to redo.");
	}

	TEST_CASE("Edit.Undo and Edit.Redo are not steps themselves")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Entity.Create", R"({"Name": "Camera"})");
		Run(Editor, "Entity.Create", R"({"Name": "Ground"})");

		Run(Editor, "Edit.Undo");
		Run(Editor, "Edit.Undo");
		Run(Editor, "Edit.Redo");
		Run(Editor, "Edit.Undo");
		Run(Editor, "Edit.Undo");

		CHECK(Editor.GetScene().GetAllEntities().empty());
		CHECK(WriteTestJson(Run(Editor, "Edit.GetHistory")) ==
			R"({"Undo":[],"Redo":["Entity.Create","Entity.Create","Entity.Create"]})");
		Run(Editor, "Edit.Redo");
		Run(Editor, "Edit.Redo");
		Run(Editor, "Edit.Redo");
		CHECK(Editor.GetScene().GetAllEntities().size() == 3);
	}

	TEST_CASE("Edit.GetHistory lists what can be undone, the most recent first, and what can be redone")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		CHECK(WriteTestJson(Run(Editor, "Edit.GetHistory")) == R"({"Undo":[],"Redo":[]})");

		Run(Editor, "Entity.Create", R"({"Name": "Player"})");
		Run(Editor, "Component.Add", R"({"Entity": 1, "Class": "WEditorTestComponent"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Hero"})");
		Run(Editor, "Edit.Undo");

		CHECK(WriteTestJson(Run(Editor, "Edit.GetHistory")) ==
			R"({"Undo":["Component.Add","Entity.Create"],"Redo":["Entity.Rename"]})");
	}
}
