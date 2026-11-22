#include "Editor.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <format>
#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("Selection commands")
{
	TEST_CASE("Selection.Get returns an empty list while nothing is selected")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Selection.Get", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) == R"({"Entities":[]})");
	}

	TEST_CASE("Selection.Set selects the given entities, and Selection.Get returns them in that order")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Editor.GetScene().CreateEntity("Ground")->GetId();
		const int64 Lamp = Editor.GetScene().CreateEntity("Lamp")->GetId();

		const FCommandResult Result = Editor.ExecuteCommand(
			"Selection.Set", ParseTestJson(std::format(R"({{"Entities": [{}, {}]}})", Lamp, Player)));

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK(Editor.GetSelection() == std::vector<int64>{Lamp, Player});
		CHECK_FALSE(Editor.IsSelected(Ground));

		const FCommandResult Selected = Editor.ExecuteCommand("Selection.Get", FJsonValue());
		REQUIRE(Selected.HasSucceeded());
		CHECK(WriteTestJson(Selected.GetValue()) == std::format(R"({{"Entities":[{},{}]}})", Lamp, Player));
	}

	TEST_CASE("Selection.Set replaces what was selected, and an empty array selects nothing")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Editor.GetScene().CreateEntity("Ground")->GetId();
		Editor.SetSelection({Player});

		REQUIRE(Editor.ExecuteCommand("Selection.Set", ParseTestJson(std::format(R"({{"Entities": [{}]}})", Ground)))
					.HasSucceeded());
		CHECK(Editor.GetSelection() == std::vector<int64>{Ground});

		REQUIRE(Editor.ExecuteCommand("Selection.Set", ParseTestJson(R"({"Entities": []})")).HasSucceeded());
		CHECK(Editor.GetSelection().empty());
	}

	TEST_CASE("Selection.Set refuses what does not select entities, and leaves what is selected")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const PlayerEntity = Editor.GetScene().CreateEntity("Player");
		const int64 Player = PlayerEntity->GetId();
		const int64 Component = PlayerEntity->AddComponent<WEditorTestComponent>()->GetId();
		Editor.SetSelection({Player});
		std::string Arguments;
		std::string ExpectedError;

		SUBCASE("Something that is not an array")
		{
			Arguments = std::format(R"({{"Entities": {}}})", Player);
			ExpectedError = "Entities must be an array of entity identifiers, but is a number.";
		}
		SUBCASE("An array that holds a name")
		{
			Arguments = R"({"Entities": ["Player"]})";
			ExpectedError = "Entities must hold entity identifiers, which are integers, but holds a string.";
		}
		SUBCASE("An array that holds a number that is not whole")
		{
			Arguments = R"({"Entities": [1.5]})";
			ExpectedError = "Entities must hold entity identifiers, which are integers, but holds a number.";
		}
		SUBCASE("An identifier that no entity has")
		{
			Arguments = R"({"Entities": [99]})";
			ExpectedError = "There is no entity with the identifier 99.";
		}
		SUBCASE("An entity that exists and one that does not")
		{
			Arguments = std::format(R"({{"Entities": [{}, 99]}})", Player);
			ExpectedError = "There is no entity with the identifier 99.";
		}
		SUBCASE("The identifier of a component")
		{
			Arguments = std::format(R"({{"Entities": [{}]}})", Component);
			ExpectedError = std::format("There is no entity with the identifier {}.", Component);
		}

		const FCommandResult Result = Editor.ExecuteCommand("Selection.Set", ParseTestJson(Arguments));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Editor.GetSelection() == std::vector<int64>{Player});
	}

	TEST_CASE("Selecting with a command is not a step of the history")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		const int64 Player = Editor.GetScene().FindEntityByName("Player")->GetId();

		REQUIRE(Editor.ExecuteCommand("Selection.Set", ParseTestJson(std::format(R"({{"Entities": [{}]}})", Player)))
					.HasSucceeded());

		const std::vector<std::string> ExpectedSteps = {"Entity.Create"};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);
	}
}
