#include "Editor.h"

#include "Assets/AssetManager.h"
#include "Commands/AssetCommands.h"
#include "Commands/CommandEffect.h"
#include "Commands/CommandWhilePlaying.h"
#include "Commands/ComponentCommands.h"
#include "Commands/EditCommands.h"
#include "Commands/EditorCommands.h"
#include "Commands/EntityCommands.h"
#include "Commands/LogCommands.h"
#include "Commands/PlayCommands.h"
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
#include "Scene/PlaySession.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

constinit FLogCategory LogEditor("LogEditor", ELogVerbosity::Log);

namespace
{
	/** A state of the history in its two parts (FEditor::WriteState). */
	struct FStateParts
	{
		/** The scene, as one line of compact JSON. */
		std::string_view SceneText;

		/** The kept assets that differ from what they were when the history began; empty if none does. */
		std::string_view AssetText;
	};

	/** Says why a command that cannot be given while a game plays (ECommandWhilePlaying::Refused) is refused. */
	std::string DescribeRefusalWhilePlaying(std::string_view CommandName)
	{
		return std::format("{} cannot be given while a game plays, because it would act on what waits unchanged until "
						   "the game stops: the scene being edited, its file, its history, the project or a material. "
						   "Play.Stop stops the game.",
			CommandName);
	}

	/** Divides a state of the history at its first line break, which only the scene's compact JSON comes before. */
	FStateParts SplitState(std::string_view State)
	{
		const std::size_t LineBreak = State.find('\n');
		if (LineBreak == std::string_view::npos)
		{
			return {State, std::string_view()};
		}
		return {State.substr(0, LineBreak), State.substr(LineBreak + 1)};
	}
}

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
	FPlayCommands::Register(Commands);

	WScene* const EmptyScene = NewObject<WScene>();
	EmptyScene->SetName(std::string(DefaultSceneName));
	SetScene(*EmptyScene, std::filesystem::path());
}

FEditor::~FEditor()
{
	// A game ends before anything else goes, its components told, and the scene being edited is back to be destroyed.
	if (IsPlaying())
	{
		StopPlay();
	}

	// Changes that were not saved are given up, and what the editor kept is left to the collector.
	EditedAssets.ReleaseAll();

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
	const bool bCanChange = Description != nullptr && Description->Effect != ECommandEffect::LeavesScene;
	const bool bIsRefused =
		IsPlaying() && Description != nullptr && Description->WhilePlaying == ECommandWhilePlaying::Refused;

	FCommandResult Result = bIsRefused ? FCommandResult::Failure(DescribeRefusalWhilePlaying(Name))
									   : Commands.Execute(*this, Name, Arguments);
	if (Result.HasSucceeded())
	{
		WW_LOG(LogEditor, Verbose, "Command {} succeeded", Name);
	}
	else
	{
		// A warning: something was asked for that did not happen. Whoever reads the log for what went wrong, in
		// the window or with Log.Read, finds it among that.
		WW_LOG(LogEditor, Warning, "Command {} failed: {}", Name, Result.GetError());
	}

	// Whatever the command changed is a step, whether it succeeded or not: a command that fails should leave the
	// scene and the assets as they were, and if one does not, what it left behind can at least be undone. Inside a step
	// that takes several commands, it is that step's to record.
	if (bCanChange && !OpenStepName.has_value())
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
	// The game was one of the scene that goes. Stopping it destroys its copy, which is then no scene to edit.
	if (IsPlaying())
	{
		StopPlay();
	}
	WW_CHECKF(IsValid(&NewScene), "The editor cannot edit a scene that has been destroyed");

	ScenePath = std::move(NewScenePath);
	if (Scene.Get() == &NewScene)
	{
		return;
	}

	ReplaceScene(NewScene);
	// What was done to the assets can no longer be undone either; what is not saved is still to be saved.
	EditedAssets.BeginHistory();
	History.Reset(WriteState());
	// A step that was open was a step of the scene that is gone.
	OpenStepName.reset();

	// The identifiers of the old scene's entities mean nothing in the new one, or other entities.
	SelectedEntityIds.clear();
}

void FEditor::SetScenePath(std::filesystem::path NewScenePath)
{
	ScenePath = std::move(NewScenePath);
}

EPlayState FEditor::GetPlayState() const
{
	if (!IsPlaying())
	{
		return EPlayState::Stopped;
	}
	return PlaySession->IsPaused() ? EPlayState::Paused : EPlayState::Playing;
}

void FEditor::StartPlay()
{
	WW_CHECKF(!IsPlaying(), "A game was started in the editor while one was playing already");

	// The game plays the scene as the history has it: what changed without a command is a step first, and a step
	// that is open ends, so that the history's current state is what the scene and the kept assets write down as.
	EndStep();
	RecordChanges(DirectChangeStepName);

	// An undo can leave the identifier of an entity that is gone among the selected, and the copy would give that
	// identifier to the next entity that the game makes, selected from the start.
	SetSelection(GetSelection());

	std::string Error;
	WScene* const Copy = FSceneSerializer::LoadFromText(SplitState(History.GetCurrentState()).SceneText, Error);
	// The state was written by WriteState, so only a mistake in the engine can make it unreadable.
	WW_CHECKF(Copy != nullptr, "The scene being edited could not be copied to play a game in: {}", Error);

	// The scene being edited stays in the root set, set aside; the session keeps the copy alive.
	EditedScene = Scene;
	Scene = Copy;
	PlaySession = std::make_unique<FPlaySession>(*Copy);
	WW_LOG(LogEditor, Log, "The game began, in a copy of the scene \"{}\".", Copy->GetName());
}

void FEditor::PausePlay()
{
	WW_CHECKF(IsPlaying(), "A game was paused in the editor while none was playing");
	PlaySession->Pause();
}

void FEditor::ResumePlay()
{
	WW_CHECKF(IsPlaying(), "A game was resumed in the editor while none was playing");
	PlaySession->Resume();
}

void FEditor::AdvancePlay(double ElapsedSeconds)
{
	if (IsPlaying())
	{
		PlaySession->Advance(ElapsedSeconds);
	}
}

void FEditor::StepPlay(int32 FrameCount)
{
	WW_CHECKF(IsPlaying(), "A game was played a frame at a time in the editor while none was playing");
	PlaySession->Step(FrameCount);
}

void FEditor::StopPlay()
{
	WW_CHECKF(IsPlaying(), "A game was stopped in the editor while none was playing");

	// A step that was begun during the game is one of the copy, which goes: it is no step of the history.
	OpenStepName.reset();

	const WScene& Played = PlaySession->GetScene();
	const int64 PlayedFrameCount = Played.GetPlayedFrameCount();
	const double PlayedSeconds = Played.GetPlayedSeconds();
	// Ends the game, every component of the copy that began play told, and destroys the copy.
	PlaySession.reset();

	WScene* const Edited = EditedScene.Get();
	WW_CHECKF(
		Edited != nullptr, "The scene being edited was destroyed by something other than the editor during a game");
	Scene = Edited;
	EditedScene.Reset();

	// What only the game had is selected no more: the scene being edited gives its identifiers out again.
	SetSelection(GetSelection());
	WW_LOG(
		LogEditor, Log, "The game ended at frame {}, after {} seconds of game time.", PlayedFrameCount, PlayedSeconds);
}

void FEditor::SetProject(FProject NewProject)
{
	// A game ends in the project it was played in: its components end play before anything of the project is given up.
	if (IsPlaying())
	{
		StopPlay();
	}

	// The assets that the editor keeps are of the content directory that is given up, and are given up with it.
	EditedAssets.ReleaseAll();
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
	// The history waits until the game stops, as the scene being edited does.
	if (IsPlaying())
	{
		return std::nullopt;
	}

	// What an open step has changed so far is what there is to undo.
	EndStep();
	RecordChanges(DirectChangeStepName);
	if (!History.CanUndo())
	{
		return std::nullopt;
	}

	std::string StepName = History.GetUndoStepNames().front();
	RestoreState(History.Undo());
	return StepName;
}

std::optional<std::string> FEditor::Redo()
{
	if (IsPlaying())
	{
		return std::nullopt;
	}

	// A change made since the undo is a step that takes the place of the ones that could have been redone.
	EndStep();
	RecordChanges(DirectChangeStepName);
	if (!History.CanRedo())
	{
		return std::nullopt;
	}

	std::string StepName = History.GetRedoStepNames().front();
	RestoreState(History.Redo());
	return StepName;
}

std::string FEditor::WriteState() const
{
	std::string State = FJsonWriter::Write(FSceneSerializer::SaveToJson(GetScene()), EJsonFormat::Compact);
	const std::string Assets = EditedAssets.WriteState();
	if (!Assets.empty())
	{
		State += '\n';
		State += Assets;
	}
	return State;
}

void FEditor::RecordChanges(std::string_view StepName)
{
	// What a game changes is the copy's, which goes when the game stops: the history waits, as the scene being edited
	// does.
	if (IsPlaying())
	{
		return;
	}
	History.Record(std::string(StepName), WriteState());
}

void FEditor::RestoreState(const std::string& State)
{
	const FStateParts Parts = SplitState(State);

	// The assets first: the scene that is loaded next finds them by their paths, as the objects that the editor keeps.
	EditedAssets.RestoreState(Parts.AssetText);

	std::string Error;
	WScene* const Restored = FSceneSerializer::LoadFromText(Parts.SceneText, Error);
	// The state was written by WriteState, so only a mistake in the engine can make it unreadable.
	WW_CHECKF(Restored != nullptr, "A state of the undo history could not be loaded: {}", Error);
	ReplaceScene(*Restored);

	// The history's current state has to be what the scene and the assets now write down as, or the next command
	// would seem to have changed them. It is the same text unless something does not survive being saved and loaded
	// exactly, or an asset's value could not be put back.
	History.ReplaceCurrentState(WriteState());
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
