#include "Editor.h"

#include "Commands/CommandEffect.h"
#include "Commands/ComponentCommands.h"
#include "Commands/EditCommands.h"
#include "Commands/EditorCommands.h"
#include "Commands/EntityCommands.h"
#include "Commands/PropertyCommands.h"
#include "Commands/SceneCommands.h"
#include "Json/JsonFormat.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <utility>

constinit FLogCategory LogEditor("LogEditor", ELogVerbosity::Log);

FEditor::FEditor()
{
	FEditorCommands::Register(Commands);
	FEditCommands::Register(Commands);
	FSceneCommands::Register(Commands);
	FEntityCommands::Register(Commands);
	FComponentCommands::Register(Commands);
	FPropertyCommands::Register(Commands);

	WScene* const EmptyScene = NewObject<WScene>();
	EmptyScene->SetName(std::string(DefaultSceneName));
	SetScene(*EmptyScene, std::filesystem::path());
}

FEditor::~FEditor()
{
	// Destroy takes the scene out of the root set. A scene that something else destroyed already is no longer there.
	if (WScene* const CurrentScene = Scene.Get())
	{
		CurrentScene->Destroy();
	}
}

FCommandResult FEditor::ExecuteCommand(std::string_view Name, const FJsonValue& Arguments)
{
	// Looked up before the command runs: a description found afterwards might not be the one that ran.
	const FCommandDescription* const Description = Commands.Find(Name);
	const bool bCanChangeScene = Description != nullptr && Description->Effect == ECommandEffect::ChangesScene;

	FCommandResult Result = Commands.Execute(*this, Name, Arguments);
	if (Result.HasSucceeded())
	{
		WW_LOG(LogEditor, Verbose, "Command {} succeeded", Name);
	}
	else
	{
		WW_LOG(LogEditor, Log, "Command {} failed: {}", Name, Result.GetError());
	}

	// Whatever the command changed is a step, whether it succeeded or not: a command that fails should leave the
	// scene as it was, and if one does not, what it left behind can at least be undone.
	if (bCanChangeScene)
	{
		RecordChanges(Name);
	}
	return Result;
}

WScene& FEditor::GetScene() const
{
	WScene* const CurrentScene = Scene.Get();
	WW_CHECKF(CurrentScene != nullptr, "The editor's scene was destroyed by something other than the editor");
	return *CurrentScene;
}

void FEditor::SetScene(WScene& NewScene, std::filesystem::path NewScenePath)
{
	WW_CHECKF(IsValid(&NewScene), "The editor cannot edit a scene that has been destroyed");

	ScenePath = std::move(NewScenePath);
	if (Scene.Get() == &NewScene)
	{
		return;
	}

	ReplaceScene(NewScene);
	History.Reset(WriteSceneState());
}

void FEditor::SetScenePath(std::filesystem::path NewScenePath)
{
	ScenePath = std::move(NewScenePath);
}

std::optional<std::string> FEditor::Undo()
{
	RecordChanges(DirectChangeStepName);
	if (!History.CanUndo())
	{
		return std::nullopt;
	}

	std::string StepName = History.GetUndoStepNames().front();
	RestoreSceneState(History.Undo());
	return StepName;
}

std::optional<std::string> FEditor::Redo()
{
	// A change made since the undo is a step that takes the place of the ones that could have been redone.
	RecordChanges(DirectChangeStepName);
	if (!History.CanRedo())
	{
		return std::nullopt;
	}

	std::string StepName = History.GetRedoStepNames().front();
	RestoreSceneState(History.Redo());
	return StepName;
}

std::string FEditor::WriteSceneState() const
{
	return FJsonWriter::Write(FSceneSerializer::SaveToJson(GetScene()), EJsonFormat::Compact);
}

void FEditor::RecordChanges(std::string_view StepName)
{
	History.Record(std::string(StepName), WriteSceneState());
}

void FEditor::RestoreSceneState(const std::string& State)
{
	std::string Error;
	WScene* const Restored = FSceneSerializer::LoadFromText(State, Error);
	// The state was written by WriteSceneState, so only a mistake in the engine can make it unreadable.
	WW_CHECKF(Restored != nullptr, "A state of the undo history could not be loaded: {}", Error);
	ReplaceScene(*Restored);

	// The history's current state has to be what the scene now writes down as, or the next command would seem to
	// have changed the scene. It is the same text unless something does not survive being saved and loaded exactly.
	History.ReplaceCurrentState(WriteSceneState());
}

void FEditor::ReplaceScene(WScene& NewScene)
{
	WScene* const OldScene = Scene.Get();
	NewScene.AddToRoot();
	Scene = &NewScene;
	if (OldScene != nullptr)
	{
		OldScene->Destroy();
	}
}
