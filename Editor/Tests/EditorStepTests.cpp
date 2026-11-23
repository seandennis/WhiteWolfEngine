#include "Editor.h"
#include "History/ScopedEditorStep.h"
#include "History/UndoHistory.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using FNames = std::vector<std::string>;

	/** Gives the editor a command with the arguments written as JSON text, which a test expects to succeed. */
	void Run(FEditor& Editor, std::string_view CommandName, std::string_view ArgumentText = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
		INFO("Command error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
	}
}

TEST_SUITE("FEditor steps of several commands")
{
	TEST_CASE("The commands between BeginStep and EndStep are one step of the history, with the step's name")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Ground"})");

		Editor.BeginStep("Create three");
		CHECK(Editor.IsStepOpen());
		Run(Editor, "Entity.Create", R"({"Name": "One"})");
		Run(Editor, "Entity.Create", R"({"Name": "Two"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 2, "Name": "First"})");
		// Nothing is a step yet.
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
		Editor.EndStep();

		CHECK_FALSE(Editor.IsStepOpen());
		const FNames ExpectedSteps = {"Create three", "Entity.Create"};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);
		CHECK(Editor.GetScene().GetAllEntities().size() == 3);

		// One undo takes back all of it, and one redo brings all of it back.
		CHECK(Editor.Undo() == std::optional<std::string>("Create three"));
		REQUIRE(Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Editor.GetScene().GetAllEntities()[0]->GetName() == "Ground");

		CHECK(Editor.Redo() == std::optional<std::string>("Create three"));
		CHECK(Editor.GetScene().GetAllEntities().size() == 3);
		CHECK(Editor.GetScene().FindEntityByName("First") != nullptr);
	}

	TEST_CASE("A step in which nothing changed is no step")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		Run(Editor, "Entity.Create", R"({"Name": "Ground"})");

		Editor.BeginStep("Nothing");
		Run(Editor, "Scene.GetHierarchy");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Floor"})");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Ground"})");
		Editor.EndStep();

		// What the step changed, it changed back: the scene is what it was, and there is nothing more to undo.
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("After a step, commands are steps of their own again")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.BeginStep("Both");
		Run(Editor, "Entity.Create", R"({"Name": "One"})");
		Run(Editor, "Entity.Create", R"({"Name": "Two"})");
		Editor.EndStep();

		Run(Editor, "Entity.Create", R"({"Name": "Three"})");

		const FNames ExpectedSteps = {"Entity.Create", "Both"};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);
	}

	TEST_CASE("What was changed without a command before a step is a step of its own")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		REQUIRE(Editor.GetScene().CreateEntity("Made directly") != nullptr);

		Editor.BeginStep("Rename");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Renamed"})");
		Editor.EndStep();

		const FNames ExpectedSteps = {"Rename", std::string(FEditor::DirectChangeStepName)};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);

		// Undoing the step undoes the renaming and not the creation.
		REQUIRE(Editor.Undo().has_value());
		REQUIRE(Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Editor.GetScene().GetAllEntities()[0]->GetName() == "Made directly");
	}

	TEST_CASE("An undo ends an open step and undoes what it had changed so far")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Ground"})");
		Editor.BeginStep("Drag");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Moved"})");

		CHECK(Editor.Undo() == std::optional<std::string>("Drag"));

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK(Editor.GetScene().GetAllEntities()[0]->GetName() == "Ground");
		const FNames ExpectedRedo = {"Drag"};
		CHECK(Editor.GetHistory().GetRedoStepNames() == ExpectedRedo);

		// The step is over: ending it again does nothing, and the next command is a step.
		Editor.EndStep();
		Run(Editor, "Entity.Create", R"({"Name": "Next"})");
		const FNames ExpectedUndo = {"Entity.Create", "Entity.Create"};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedUndo);
	}

	TEST_CASE("A redo ends an open step, which takes the place of what could have been redone")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Ground"})");
		Run(Editor, "Entity.Create", R"({"Name": "Tree"})");
		REQUIRE(Editor.Undo().has_value());
		Editor.BeginStep("Rename");
		Run(Editor, "Entity.Rename", R"({"Entity": 1, "Name": "Floor"})");

		CHECK_FALSE(Editor.Redo().has_value());

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK(Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Editor.GetScene().GetAllEntities()[0]->GetName() == "Floor");
		const FNames ExpectedUndo = {"Rename", "Entity.Create"};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedUndo);
	}

	TEST_CASE("A scene that replaces the one being edited ends an open step, with nothing to undo")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.BeginStep("Lost");
		Run(Editor, "Entity.Create", R"({"Name": "One"})");

		Run(Editor, "Scene.New");

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK_FALSE(Editor.GetHistory().CanUndo());

		// The next command in the new scene is a step like any other.
		Run(Editor, "Entity.Create", R"({"Name": "Two"})");
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("Ending a step that was not begun does nothing")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "One"})");

		Editor.EndStep();

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Entity.Create"});
	}

	TEST_CASE("Beginning a step while another is open is a fatal error")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		Editor.BeginStep("First");

		CHECK_THROWS_AS(Editor.BeginStep("Second"), FFatalErrorException);

		// The first step is still the one that is open.
		Run(Editor, "Entity.Create", R"({"Name": "One"})");
		Editor.EndStep();
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"First"});
	}

	TEST_CASE("FScopedEditorStep makes the commands of its scope one step")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		{
			const FScopedEditorStep Step(Editor, "Scoped");
			CHECK(Editor.IsStepOpen());
			Run(Editor, "Entity.Create", R"({"Name": "One"})");
			Run(Editor, "Entity.Create", R"({"Name": "Two"})");
		}

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Scoped"});
	}
}
