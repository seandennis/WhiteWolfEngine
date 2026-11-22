#include "Editor.h"

#include "Commands/ComponentCommands.h"
#include "Commands/EditorCommands.h"
#include "Commands/EntityCommands.h"
#include "Commands/PropertyCommands.h"
#include "Commands/SceneCommands.h"
#include "Json/JsonValue.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Scene.h"

#include <string>
#include <utility>

constinit FLogCategory LogEditor("LogEditor", ELogVerbosity::Log);

FEditor::FEditor()
{
	FEditorCommands::Register(Commands);
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
	FCommandResult Result = Commands.Execute(*this, Name, Arguments);
	if (Result.HasSucceeded())
	{
		WW_LOG(LogEditor, Verbose, "Command {} succeeded", Name);
	}
	else
	{
		WW_LOG(LogEditor, Log, "Command {} failed: {}", Name, Result.GetError());
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

	WScene* const OldScene = Scene.Get();
	if (OldScene == &NewScene)
	{
		ScenePath = std::move(NewScenePath);
		return;
	}

	NewScene.AddToRoot();
	Scene = &NewScene;
	ScenePath = std::move(NewScenePath);

	if (OldScene != nullptr)
	{
		OldScene->Destroy();
	}
}

void FEditor::SetScenePath(std::filesystem::path NewScenePath)
{
	ScenePath = std::move(NewScenePath);
}
