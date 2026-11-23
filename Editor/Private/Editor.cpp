#include "Editor.h"

#include "Assets/AssetManager.h"
#include "Commands/AssetCommands.h"
#include "Commands/CommandEffect.h"
#include "Commands/ComponentCommands.h"
#include "Commands/EditCommands.h"
#include "Commands/EditorCommands.h"
#include "Commands/EntityCommands.h"
#include "Commands/LogCommands.h"
#include "Commands/ProjectCommands.h"
#include "Commands/PropertyCommands.h"
#include "Commands/SceneCommands.h"
#include "Commands/SelectionCommands.h"
#include "Commands/ViewportCommands.h"
#include "Core/FileHelper.h"
#include "Json/JsonFormat.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Logging/Log.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <memory>
#include <system_error>
#include <utility>

constinit FLogCategory LogEditor("LogEditor", ELogVerbosity::Log);

FEditor::FEditor()
	: LogSink(std::make_shared<FMemoryLogSink>())
{
	// Before anything else, so that whatever the editor does is in its log.
	FLog::AddSink(LogSink);

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
	FLogCommands::Register(Commands);

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

	FLog::RemoveSink(LogSink);
}

void FEditor::ClearLog()
{
	LogSink->Clear();
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
	// scene as it was, and if one does not, what it left behind can at least be undone. Inside a step that takes
	// several commands, it is that step's to record.
	if (bCanChangeScene && !OpenStepName.has_value())
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
	// A step that was open was a step of the scene that is gone.
	OpenStepName.reset();

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

	// Another editor may have written the settings since this one read them: what it remembered is kept.
	ReadSettings();
	Settings.AddRecentProject(Project->GetFile());
	WriteSettings();

	// The scene of another project, or of none, has nothing to do with this one's assets.
	WScene* const EmptyScene = NewObject<WScene>();
	EmptyScene->SetName(std::string(DefaultSceneName));
	SetScene(*EmptyScene, std::filesystem::path());
}

std::vector<FProject> FEditor::FindRecentProjects() const
{
	std::vector<FProject> RecentProjects;
	for (const std::filesystem::path& File : Settings.GetRecentProjects())
	{
		// Why a project cannot be opened does not matter here: it is not one to offer.
		std::string IgnoredError;
		if (std::optional<FProject> RecentProject = FProject::Open(File, IgnoredError))
		{
			RecentProjects.push_back(std::move(*RecentProject));
		}
	}
	return RecentProjects;
}

void FEditor::SetSettingsFile(std::filesystem::path File)
{
	SettingsFile = std::move(File);
	ReadSettings();
}

void FEditor::ReadSettings()
{
	// Settings that were never written are no failure: the editor has not remembered anything yet.
	std::error_code ErrorCode;
	if (SettingsFile.empty() || !std::filesystem::exists(SettingsFile, ErrorCode))
	{
		return;
	}

	std::string Error;
	std::optional<FEditorSettings> Loaded = FEditorSettings::Load(SettingsFile, Error);
	if (Loaded.has_value())
	{
		Settings = std::move(*Loaded);
	}
	else
	{
		WW_LOG(LogEditor, Warning, "The editor's settings were not read: {}", Error);
	}
}

void FEditor::WriteSettings() const
{
	std::string Error;
	if (!SettingsFile.empty() && !Settings.Save(SettingsFile, Error))
	{
		WW_LOG(LogEditor, Warning, "The editor's settings were not written: {}", Error);
	}
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

void FEditor::SetGizmoSettings(const FGizmoSettings& NewSettings)
{
	WW_CHECKF(NewSettings.IsValid(), "The gizmo of the viewport was given a step that is no number or out of range");
	GizmoSettings = NewSettings;
}

std::filesystem::path FEditor::ResolveFilePath(const std::filesystem::path& Path) const
{
	if (Project.has_value() && !Path.empty() && Path.is_relative())
	{
		return Project->GetContentDirectory() / Path;
	}
	return Path;
}

std::string FEditor::MakeProjectPath(const std::filesystem::path& File) const
{
	if (Project.has_value() && File.is_absolute())
	{
		// A path that leads out of the content directory begins with "..", and is no path within it.
		const std::filesystem::path Relative = File.lexically_relative(Project->GetContentDirectory());
		const bool bIsInContent = !Relative.empty() && Relative.native().front() != '.';
		if (bIsInContent)
		{
			const std::u8string Text = Relative.generic_u8string();
			return std::string(Text.begin(), Text.end());
		}
	}
	return FFileHelper::PathToUtf8(File);
}

void FEditor::BeginStep(std::string StepName)
{
	WW_CHECKF(!OpenStepName.has_value(),
		"The step '{}' of the history was begun while the step '{}' was still open",
		StepName,
		*OpenStepName);

	// What changed without a command before the step is not part of it.
	RecordChanges(DirectChangeStepName);
	OpenStepName = std::move(StepName);
}

void FEditor::EndStep()
{
	if (OpenStepName.has_value())
	{
		const std::string StepName = std::move(*OpenStepName);
		OpenStepName.reset();
		RecordChanges(StepName);
	}
}

std::optional<std::string> FEditor::Undo()
{
	// What an open step has changed so far is what there is to undo.
	EndStep();
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
	EndStep();
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
