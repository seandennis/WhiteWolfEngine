#include "Editor.h"

#include "Assets/AssetManager.h"
#include "Commands/AssetCommands.h"
#include "Commands/CommandEffect.h"
#include "Commands/ComponentCommands.h"
#include "Commands/EditCommands.h"
#include "Commands/EditorCommands.h"
#include "Commands/EntityCommands.h"
#include "Commands/ProjectCommands.h"
#include "Commands/PropertyCommands.h"
#include "Commands/SceneCommands.h"
#include "Commands/SelectionCommands.h"
#include "Commands/ViewportCommands.h"
#include "Json/JsonFormat.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <utility>

constinit FLogCategory LogEditor("LogEditor", ELogVerbosity::Log);

FEditor::FEditor()
{
	FEditorCommands::Register(Commands);
	FEditCommands::Register(Commands);
	FProjectCommands::Register(Commands);
	FAssetCommands::Register(Commands);
	FSceneCommands::Register(Commands);
	FEntityCommands::Register(Commands);
	FComponentCommands::Register(Commands);
	FPropertyCommands::Register(Commands);
	FSelectionCommands::Register(Commands);
	FViewportCommands::Register(Commands);

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

	// The content directory was this editor's project's.
	if (Project.has_value())
	{
		FAssetManager::SetContentDirectory(std::filesystem::path());
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

	// The identifiers of the old scene's entities mean nothing in the new one, or other entities.
	SelectedEntityIds.clear();
}

void FEditor::SetScenePath(std::filesystem::path NewScenePath)
{
	ScenePath = std::move(NewScenePath);
}

void FEditor::SetProject(FProject NewProject)
{
	FAssetManager::SetContentDirectory(NewProject.GetContentDirectory());
	Project = std::move(NewProject);

	// The scene of another project, or of none, has nothing to do with this one's assets.
	WScene* const EmptyScene = NewObject<WScene>();
	EmptyScene->SetName(std::string(DefaultSceneName));
	SetScene(*EmptyScene, std::filesystem::path());
}

std::vector<int64> FEditor::GetSelection() const
{
	const WScene& CurrentScene = GetScene();
	std::vector<int64> Selection;
	for (const int64 EntityId : SelectedEntityIds)
	{
		if (CurrentScene.FindEntityById(EntityId) != nullptr)
		{
			Selection.push_back(EntityId);
		}
	}
	return Selection;
}

bool FEditor::IsSelected(int64 EntityId) const
{
	const bool bIsAmongSelected = std::ranges::find(SelectedEntityIds, EntityId) != SelectedEntityIds.end();
	return bIsAmongSelected && GetScene().FindEntityById(EntityId) != nullptr;
}

void FEditor::SetSelection(const std::vector<int64>& EntityIds)
{
	const WScene& CurrentScene = GetScene();
	SelectedEntityIds.clear();
	for (const int64 EntityId : EntityIds)
	{
		const bool bIsNew = std::ranges::find(SelectedEntityIds, EntityId) == SelectedEntityIds.end();
		if (bIsNew && CurrentScene.FindEntityById(EntityId) != nullptr)
		{
			SelectedEntityIds.push_back(EntityId);
		}
	}
}

void FEditor::SetViewportCamera(const FViewportCamera& NewCamera)
{
	WW_CHECKF(NewCamera.IsValid(), "The camera of the viewport was given numbers that no picture can be drawn from");
	ViewportCamera = NewCamera;
}

std::filesystem::path FEditor::ResolveFilePath(const std::filesystem::path& Path) const
{
	if (Project.has_value() && !Path.empty() && Path.is_relative())
	{
		return Project->GetContentDirectory() / Path;
	}
	return Path;
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
