#include "Editor.h"

#include "Commands/CommandResult.h"
#include "Json/JsonValue.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Gives the editor a command with arguments written as JSON text, which the test expects to succeed. */
	FJsonValue Execute(FEditor& Editor, std::string_view Name, std::string_view ArgumentText = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand(Name, ParseTestJson(ArgumentText));
		INFO("Command ", std::string(Name), ": ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result.GetValue();
	}

	/** Gives the editor a command that the test expects to fail, and returns why it failed. */
	std::string ExecuteFailing(FEditor& Editor, std::string_view Name, std::string_view ArgumentText = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand(Name, ParseTestJson(ArgumentText));
		REQUIRE_FALSE(Result.HasSucceeded());
		return Result.GetError();
	}

	/**
	 * Puts an entity with a mover into the scene being edited, and returns the identifier of the mover. The mover
	 * moves its entity one metre along X in a frame of a sixtieth of a second, exactly.
	 */
	int64 AddMover(FEditor& Editor)
	{
		WEntity* const Entity = Editor.GetScene().CreateEntity("Mover");
		REQUIRE(Entity != nullptr);
		return Entity->AddComponent<WEditorTestMoverComponent>()->GetId();
	}

	/** Returns the state of play that a command returned, as compact JSON. */
	std::string DescribeState(std::string_view State, int64 Frame)
	{
		return std::format(R"({{"State":"{}","Frame":{},"Time":{},"FrameSeconds":{}}})",
			State,
			Frame,
			WriteTestJson(FJsonValue(static_cast<double>(Frame) * WScene::DefaultFrameSeconds)),
			WriteTestJson(FJsonValue(WScene::DefaultFrameSeconds)));
	}
}

TEST_SUITE("Play commands")
{
	TEST_CASE("Play.Start plays a copy of the scene being edited and returns the state at frame 0")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		AddMover(Editor);
		const WScene* const Edited = &Editor.GetScene();

		const FJsonValue State = Execute(Editor, "Play.Start");

		CHECK(WriteTestJson(State) == DescribeState("Playing", 0));
		CHECK(Editor.IsPlaying());
		CHECK(&Editor.GetScene() != Edited);
		CHECK(FEditorTestMoverEvents::Get().BeginPlayCount == 1);
		Editor.StopPlay();
	}

	TEST_CASE("Play.Start fails while a game plays, and changes nothing")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		AddMover(Editor);
		Execute(Editor, "Play.Start");
		Execute(Editor, "Play.Step", R"({"Frames": 2})");
		const WScene* const Copy = &Editor.GetScene();

		CHECK(ExecuteFailing(Editor, "Play.Start") == "A game is playing already. Play.Stop stops it.");

		CHECK(&Editor.GetScene() == Copy);
		CHECK(Copy->GetPlayedFrameCount() == 2);
		CHECK(FEditorTestMoverEvents::Get().BeginPlayCount == 1);
		Editor.StopPlay();
	}

	TEST_CASE("Play.Step plays the frames it is given, and one if it is given none")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = AddMover(Editor);
		const int64 EntityId = Editor.GetScene().FindComponentById(MoverId)->GetOwner()->GetId();
		Execute(Editor, "Play.Start");

		CHECK(WriteTestJson(Execute(Editor, "Play.Step")) == DescribeState("Playing", 1));
		const FJsonValue State = Execute(Editor, "Play.Step", R"({"Frames": 119})");

		CHECK(WriteTestJson(State) == DescribeState("Playing", 120));
		CHECK(State.FindMember("Time")->GetNumber() == 2.0);
		CHECK(WriteTestJson(Execute(Editor,
				  "Object.GetProperty",
				  std::format(R"({{"Object": {}, "Property": "FrameCount"}})", MoverId))) == "120");
		CHECK(Editor.GetScene().FindEntityById(EntityId)->GetRelativeTransform().Location == FVector(120.0, 0.0, 0.0));
		Editor.StopPlay();
	}

	TEST_CASE("Play.Step plays from 1 to 3600 frames, and refuses any other number with a sentence")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Execute(Editor, "Play.Start");

		for (const int64 Frames : {int64{0}, int64{-1}, int64{3601}})
		{
			CAPTURE(Frames);
			CHECK(ExecuteFailing(Editor, "Play.Step", std::format(R"({{"Frames": {}}})", Frames)) ==
				std::format("Frames must be from 1 to 3600, a minute of the game's time, and {} was given.", Frames));
		}
		CHECK(ExecuteFailing(Editor, "Play.Step", R"({"Frames": 1.5})") ==
			"The argument \"Frames\" of the command \"Play.Step\" must be an integer, but it is a number that is not "
			"written as an integer.");
		CHECK(ExecuteFailing(Editor, "Play.Step", R"({"Frames": "2"})") ==
			"The argument \"Frames\" of the command \"Play.Step\" must be an integer, but it is a string.");
		CHECK(Editor.GetScene().GetPlayedFrameCount() == 0);

		CHECK(WriteTestJson(Execute(Editor, "Play.Step", R"({"Frames": 3600})")) == DescribeState("Playing", 3600));
		Editor.StopPlay();
	}

	TEST_CASE("Play.Step plays a paused game, which stays paused")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Execute(Editor, "Play.Start");
		Execute(Editor, "Play.Pause");

		const FJsonValue State = Execute(Editor, "Play.Step", R"({"Frames": 3})");

		CHECK(WriteTestJson(State) == DescribeState("Paused", 3));
		Editor.StopPlay();
	}

	TEST_CASE("Play.Pause and Play.Resume change the state, and asking for the state there is changes nothing")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Execute(Editor, "Play.Start");

		CHECK(WriteTestJson(Execute(Editor, "Play.Pause")) == DescribeState("Paused", 0));
		CHECK(WriteTestJson(Execute(Editor, "Play.Pause")) == DescribeState("Paused", 0));
		CHECK(WriteTestJson(Execute(Editor, "Play.GetState")) == DescribeState("Paused", 0));
		CHECK(WriteTestJson(Execute(Editor, "Play.Resume")) == DescribeState("Playing", 0));
		CHECK(WriteTestJson(Execute(Editor, "Play.Resume")) == DescribeState("Playing", 0));
		CHECK(WriteTestJson(Execute(Editor, "Play.GetState")) == DescribeState("Playing", 0));
		Editor.StopPlay();
	}

	TEST_CASE("Play.Stop ends the game, gives the scene being edited back and says how long the game played")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		AddMover(Editor);
		const WScene* const Edited = &Editor.GetScene();
		Execute(Editor, "Play.Start");
		Execute(Editor, "Play.Step", R"({"Frames": 120})");

		const FJsonValue Result = Execute(Editor, "Play.Stop");

		CHECK(WriteTestJson(Result) == R"({"State":"Stopped","PlayedFrames":120,"PlayedSeconds":2.0})");
		CHECK_FALSE(Editor.IsPlaying());
		CHECK(&Editor.GetScene() == Edited);
		CHECK(FEditorTestMoverEvents::Get().EndPlayCount == 1);
		CHECK(WriteTestJson(Execute(Editor, "Play.GetState")) == R"({"State":"Stopped"})");
	}

	TEST_CASE("Without a game the play commands fail with a sentence, and Play.GetState says that there is none")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		for (const std::string_view Name : {"Play.Pause", "Play.Resume", "Play.Step"})
		{
			CAPTURE(Name);
			CHECK(ExecuteFailing(Editor, Name) == "No game is playing. Play.Start starts one.");
		}
		CHECK(ExecuteFailing(Editor, "Play.Stop") == "No game is playing, so there is nothing to stop.");
		CHECK(WriteTestJson(Execute(Editor, "Play.GetState")) == R"({"State":"Stopped"})");
		CHECK_FALSE(Editor.IsPlaying());
	}

	TEST_CASE("No play command is a step of the history")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		AddMover(Editor);
		Execute(Editor, "Entity.Create", R"({"Name": "Before"})");
		const std::vector<std::string> UndoSteps = Editor.GetHistory().GetUndoStepNames();

		Execute(Editor, "Play.Start");
		Execute(Editor, "Play.Step", R"({"Frames": 10})");
		Execute(Editor, "Play.Pause");
		Execute(Editor, "Play.Resume");
		Execute(Editor, "Play.GetState");
		Execute(Editor, "Play.Stop");

		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoSteps);
		CHECK(Editor.GetHistory().GetRedoStepNames().empty());
	}

	TEST_CASE("Scene.GetHierarchy says whether a game plays, and shows the scene that the commands act on")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Execute(Editor, "Entity.Create", R"({"Name": "Edited"})");
		Execute(Editor, "Play.Start");
		Execute(Editor, "Entity.Create", R"({"Name": "Made in the game"})");

		const FJsonValue During = Execute(Editor, "Scene.GetHierarchy");

		CHECK(During.FindMember("Playing")->GetBoolean());
		CHECK(During.FindMember("Entities")->GetElements().size() == 2);
		Execute(Editor, "Play.Stop");
		const FJsonValue After = Execute(Editor, "Scene.GetHierarchy");
		CHECK_FALSE(After.FindMember("Playing")->GetBoolean());
		REQUIRE(After.FindMember("Entities")->GetElements().size() == 1);
		CHECK(After.FindMember("Entities")->GetElements()[0].FindMember("Name")->GetString() == "Edited");
	}

	TEST_CASE("The same commands on the same scene give the same game")
	{
		const FScopedEditorTest EditorTest;
		std::vector<std::string> Results;
		for (int32 Run = 0; Run < 2; ++Run)
		{
			FEditor Editor;
			const int64 MoverId = AddMover(Editor);
			Execute(Editor,
				"Object.SetProperty",
				std::format(R"({{"Object": {}, "Property": "Velocity", "Value": [1.1, -2.3, 0.7]}})", MoverId));
			Execute(Editor, "Play.Start");
			Execute(Editor, "Play.Step", R"({"Frames": 120})");

			const FJsonValue Hierarchy = Execute(Editor, "Scene.GetHierarchy");
			const FJsonValue Entity = Execute(Editor,
				"Object.GetProperties",
				std::format(R"({{"Object": {}}})", Editor.GetScene().FindComponentById(MoverId)->GetOwner()->GetId()));
			const FJsonValue Mover =
				Execute(Editor, "Object.GetProperties", std::format(R"({{"Object": {}}})", MoverId));
			Results.push_back(WriteTestJson(Hierarchy) + WriteTestJson(Entity) + WriteTestJson(Mover));
			Execute(Editor, "Play.Stop");
		}

		CHECK(Results[0] == Results[1]);
		CHECK(Results[0].find(R"("FrameCount":120)") != std::string::npos);
	}
}
