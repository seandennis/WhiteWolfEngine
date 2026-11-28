#include "Commands/PlayCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Play/PlayState.h"
#include "Scene/Scene.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	/** The most frames that Play.Step plays at once: a minute of the game's time, in frames of a sixtieth of a second.
	 */
	constexpr int64 MaxStepFrames = 3600;

	/** What a command that needs a game says without one. */
	constexpr std::string_view NoGameError = "No game is playing. Play.Start starts one.";

	/** Returns the name of a state of play, as the commands write it. */
	std::string_view GetStateName(EPlayState State)
	{
		switch (State)
		{
			case EPlayState::Playing:
				return "Playing";
			case EPlayState::Paused:
				return "Paused";
			case EPlayState::Stopped:
			default:
				return "Stopped";
		}
	}

	/**
	 * Returns the state of play: whether a game exists and is paused, and while it exists, the frames played, the
	 * game time they make and the length of a frame.
	 */
	FJsonValue DescribePlayState(const FEditor& Editor)
	{
		const EPlayState State = Editor.GetPlayState();
		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("State", std::string(GetStateName(State)));
		if (State != EPlayState::Stopped)
		{
			const WScene& Played = Editor.GetScene();
			Value.SetMember("Frame", Played.GetPlayedFrameCount());
			Value.SetMember("Time", Played.GetPlayedSeconds());
			Value.SetMember("FrameSeconds", Played.GetFrameSeconds());
		}
		return Value;
	}

	FCommandResult StartPlay(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		if (Editor.IsPlaying())
		{
			return FCommandResult::Failure("A game is playing already. Play.Stop stops it.");
		}

		Editor.StartPlay();
		return FCommandResult::Success(DescribePlayState(Editor));
	}

	FCommandResult StepPlay(FEditor& Editor, const FCommandArguments& Arguments)
	{
		if (!Editor.IsPlaying())
		{
			return FCommandResult::Failure(std::string(NoGameError));
		}
		const int64 FrameCount = Arguments.Has("Frames") ? Arguments.GetInteger("Frames") : 1;
		if (FrameCount < 1 || FrameCount > MaxStepFrames)
		{
			return FCommandResult::Failure(
				std::format("Frames must be from 1 to {}, a minute of the game's time, and {} was given.",
					MaxStepFrames,
					FrameCount));
		}

		Editor.StepPlay(static_cast<int32>(FrameCount));
		return FCommandResult::Success(DescribePlayState(Editor));
	}

	FCommandResult PausePlay(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		if (!Editor.IsPlaying())
		{
			return FCommandResult::Failure(std::string(NoGameError));
		}

		Editor.PausePlay();
		return FCommandResult::Success(DescribePlayState(Editor));
	}

	FCommandResult ResumePlay(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		if (!Editor.IsPlaying())
		{
			return FCommandResult::Failure(std::string(NoGameError));
		}

		Editor.ResumePlay();
		return FCommandResult::Success(DescribePlayState(Editor));
	}

	FCommandResult StopPlay(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		if (!Editor.IsPlaying())
		{
			return FCommandResult::Failure("No game is playing, so there is nothing to stop.");
		}

		const WScene& Played = Editor.GetScene();
		const int64 PlayedFrames = Played.GetPlayedFrameCount();
		const double PlayedSeconds = Played.GetPlayedSeconds();
		Editor.StopPlay();

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("State", std::string(GetStateName(EPlayState::Stopped)));
		Value.SetMember("PlayedFrames", PlayedFrames);
		Value.SetMember("PlayedSeconds", PlayedSeconds);
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult GetPlayState(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		return FCommandResult::Success(DescribePlayState(Editor));
	}
}

void FPlayCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Play.Start",
			"Starts the game: plays a copy of the scene being edited, in which every component begins play, while the "
			"scene being edited waits exactly as it is. Until Play.Stop, every command that reads or changes the scene "
			"acts on the game's copy, which has the same identifiers, and nothing is a step of the history; the "
			"commands that would act on what waits unchanged are refused (Editor.ListCommands says which, "
			"\"AllowedWhilePlaying\"). The game runs in frames of a sixtieth of a second of game time; in an agent's "
			"session only Play.Step runs them, so the same commands always give the same game. Returns the game's "
			"state, as Play.GetState does.",
			{},
			ECommandEffect::LeavesScene},
		StartPlay);

	Registry.Register(
		{"Play.Step",
			"Plays frames of the game now, paused or not, and returns its state after them. A frame is a fixed "
			"sixtieth of a second of game time (\"FrameSeconds\"), so the same frames of the same scene always give "
			"the same game.",
			{FCommandParameter::Optional("Frames",
				ECommandArgumentType::Integer,
				"How many frames to play, from 1 to 3600 (a minute of game time). One if left out.")},
			ECommandEffect::LeavesScene},
		StepPlay);

	Registry.Register(
		{"Play.Pause",
			"Pauses the game: in the editor's window, time no longer passes for it; Play.Step still plays frames. In "
			"an agent's session only Play.Step plays frames, so only the state changes. Returns the game's state; "
			"pausing a paused game changes nothing.",
			{},
			ECommandEffect::LeavesScene},
		PausePlay);

	Registry.Register(
		{"Play.Resume",
			"Lets a paused game go on with the time that passes in the editor's window. In an agent's session only "
			"Play.Step plays frames, so only the state changes. Returns the game's state; resuming a game that is not "
			"paused changes nothing.",
			{},
			ECommandEffect::LeavesScene},
		ResumePlay);

	Registry.Register(
		{"Play.Stop",
			"Stops the game: every component that began play ends it, the game's copy is thrown away with everything "
			"done to it, and the scene being edited is shown again exactly as it was when the game started; what is "
			"selected stays selected where the scene being edited has it. The viewport's camera stays where it is. "
			"Identifiers of entities made during the game mean nothing afterwards, and are given to other entities. "
			"Returns how many frames were played (\"PlayedFrames\") and the game time they made (\"PlayedSeconds\").",
			{},
			ECommandEffect::LeavesScene},
		StopPlay);

	Registry.Register(
		{"Play.GetState",
			"Returns whether a game exists: \"State\" is \"Stopped\", \"Playing\" or \"Paused\"; while there is a "
			"game, \"Frame\" is the number of frames played, \"Time\" the seconds of game time they make, and "
			"\"FrameSeconds\" the length of a frame.",
			{},
			ECommandEffect::LeavesScene},
		GetPlayState);
}
