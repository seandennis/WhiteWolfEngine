#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandResult.h"
#include "History/UndoHistory.h"
#include "Object/WeakObjectPtr.h"
#include "Project/Project.h"
#include "Rendering/SceneViewMode.h"
#include "Settings/EditorSettings.h"
#include "Viewport/ViewportCamera.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class FJsonValue;
class WScene;

/** Log category of the editor. */
extern EDITOR_API FLogCategory LogEditor;

/**
 * The editor without its windows: the scene being edited and the commands that change it.
 *
 * Everything the editor does is a command (see FCommandRegistry), and ExecuteCommand is how the user interface, the
 * tests and the agent interface all make it do something. An FEditor needs no window, graphics card or user, so
 * the whole of the editor's behaviour can be driven and tested unattended.
 *
 * An editor always has a scene: an empty one called "Untitled" to begin with. The editor keeps its scene alive.
 * It may have a project (FProject), which is where the assets that scenes refer to are. It knows which entities
 * of the scene are selected, the ones that the user is working on, and from where the scene is looked at: the
 * camera of the viewport, which is the user's eye and not something in the scene. And it remembers which projects
 * were opened last (FEditorSettings), from one run to the next if it is given a file to keep that in.
 *
 * Every change to the scene can be undone. After each command that can change the scene (ECommandEffect), the
 * editor writes the scene down, and if it differs from what it was, that is a step of the history; Undo and Redo
 * put the scene of an earlier or later step back. A command therefore has nothing to do to be undoable. Undo and Redo
 * replace the scene object with one that was loaded from what was written down, so its entities and components are new
 * objects with the identifiers they had: refer to them by identifier, not by pointer, from one command to the next.
 *
 * Create and destroy an editor on the main thread.
 */
class FEditor
{
public:
	/** The name of a new scene that was not given one. */
	static constexpr std::string_view DefaultSceneName = "Untitled";

	/** The name of a step of the history that is a change made to the scene without a command. */
	static constexpr std::string_view DirectChangeStepName = "Direct change";

	/** Creates an editor with an empty scene and the editor's own commands. */
	EDITOR_API FEditor();

	/** Destroys the editor's scene. */
	EDITOR_API ~FEditor();

	FEditor(const FEditor&) = delete;
	FEditor& operator=(const FEditor&) = delete;

	/** Returns the editor's commands. Register a command here to add something the editor can do. */
	FCommandRegistry& GetCommands()
	{
		return Commands;
	}

	const FCommandRegistry& GetCommands() const
	{
		return Commands;
	}

	/**
	 * Carries out a command on this editor; see FCommandRegistry::Execute. If the command can change the scene and
	 * the scene is different afterwards, the difference becomes a step of the history, named after the command, that
	 * Undo can undo.
	 *
	 * @param Arguments A JSON object with one member per argument, or JSON null for no arguments.
	 */
	EDITOR_API FCommandResult ExecuteCommand(std::string_view Name, const FJsonValue& Arguments);

	/**
	 * Returns the scene being edited. Do not keep the reference from one command to the next: Undo, Redo and
	 * SetScene replace the scene object.
	 */
	EDITOR_API WScene& GetScene() const;

	/**
	 * Makes another scene the one being edited, and destroys the scene it replaces with everything in it. The
	 * history starts anew: what was done to the old scene can no longer be undone.
	 *
	 * @param NewScene A valid scene. The editor keeps it alive from now on. Only the editor may destroy it.
	 * @param NewScenePath The file NewScene was loaded from; empty for a scene that has no file yet.
	 */
	EDITOR_API void SetScene(WScene& NewScene, std::filesystem::path NewScenePath);

	/**
	 * Undoes the last step of the history: puts the scene back as it was before it. A change made to the scene
	 * without a command since the last step is a step of its own, called DirectChangeStepName, and is what is undone.
	 *
	 * @return The name of the step that was undone, or nothing if there was no step to undo.
	 */
	EDITOR_API std::optional<std::string> Undo();

	/**
	 * Redoes the step that was undone last: puts the scene back as it was after it. Nothing can be redone once the
	 * scene has changed since the undo.
	 *
	 * @return The name of the step that was redone, or nothing if there was no step to redo.
	 */
	EDITOR_API std::optional<std::string> Redo();

	/**
	 * Begins a step of the history that is made of several commands. Until EndStep the commands that are given
	 * change the scene as always but are not steps by themselves, and EndStep makes everything that has changed
	 * one step with the given name: what the user did as one thing, such as deleting three entities or dragging
	 * one across the viewport, is undone as one thing. A change made since the last step is a step of its own
	 * first, as it is before an undo.
	 *
	 * Steps do not nest: beginning one while another is open is a fatal error. Undo, Redo and a scene that
	 * replaces the one being edited end an open step.
	 */
	EDITOR_API void BeginStep(std::string StepName);

	/**
	 * Ends the step that BeginStep began: what has changed since is one step of the history, or none if nothing
	 * has. Does nothing if no step is open.
	 */
	EDITOR_API void EndStep();

	/** Returns true between BeginStep and what ends the step. */
	bool IsStepOpen() const
	{
		return OpenStepName.has_value();
	}

	/** Returns the steps that can be undone and redone. */
	const FUndoHistory& GetHistory() const
	{
		return History;
	}

	/** Returns the file the scene was opened from or last saved to. Empty for a scene that has no file yet. */
	const std::filesystem::path& GetScenePath() const
	{
		return ScenePath;
	}

	/** Records the file the scene now belongs to, after it has been saved there. */
	EDITOR_API void SetScenePath(std::filesystem::path NewScenePath);

	/**
	 * Returns the identifiers of the selected entities, in the order they were selected. What is shown for one
	 * entity, as its properties are, is shown for the last of them. An entity that no longer exists is not among
	 * them, and is selected again if an undo brings it back.
	 */
	EDITOR_API std::vector<int64> GetSelection() const;

	/** Returns true if the entity with the given identifier exists and is selected. */
	EDITOR_API bool IsSelected(int64 EntityId) const;

	/**
	 * Selects the given entities and no others. What is selected is not part of the scene: selecting is not a step
	 * of the history, and a scene that replaces the one being edited (SetScene) starts with nothing selected.
	 *
	 * @param EntityIds The identifiers of the entities, in the order they count as selected. An identifier that is
	 * given more than once counts where it is given first, and one that no entity of the scene has is ignored.
	 */
	EDITOR_API void SetSelection(const std::vector<int64>& EntityIds);

	/** Returns the point of view from which the viewport shows the scene. */
	const FViewportCamera& GetViewportCamera() const
	{
		return ViewportCamera;
	}

	/**
	 * Moves the camera of the viewport. It is the user's point of view and not part of the scene, so moving it is
	 * not a step of the history, and it stays where it is when another scene is opened.
	 *
	 * @param NewCamera A valid camera (FViewportCamera::IsValid). Anything else is a fatal error.
	 */
	EDITOR_API void SetViewportCamera(const FViewportCamera& NewCamera);

	/** Returns how the viewport shades the scene. */
	ESceneViewMode GetViewMode() const
	{
		return ViewMode;
	}

	/** Makes the viewport shade the scene in another way. Like the camera, this is not part of the scene. */
	void SetViewMode(ESceneViewMode NewViewMode)
	{
		ViewMode = NewViewMode;
	}

	/** Returns the project being worked on, or null while none is open. */
	const FProject* GetProject() const
	{
		return Project.has_value() ? &*Project : nullptr;
	}

	/**
	 * Makes a project the one being worked on. Its content directory becomes the directory that asset paths are
	 * relative to (FAssetManager), and the scene is replaced by an empty one, with a history that starts anew. The
	 * editor gives the content directory up again when it is destroyed. The project becomes the first of the
	 * projects that the settings remember as opened last.
	 */
	EDITOR_API void SetProject(FProject NewProject);

	/** Returns what the editor remembers from one run to the next; see SetSettingsFile. */
	const FEditorSettings& GetSettings() const
	{
		return Settings;
	}

	/**
	 * Returns those of the projects that were opened last which are still there to be opened, the most recent
	 * first: what to offer a user who wants to go on where they left off. A project that was moved or deleted
	 * since is not among them, but is not forgotten either, and is back when its file is.
	 *
	 * The project files are read to find out. Ask when the answer is needed, not in every frame.
	 */
	EDITOR_API std::vector<FProject> FindRecentProjects() const;

	/**
	 * Has the editor keep its settings in a file: they are read from it now, if it is there, and written to it
	 * whenever they change. Before they change they are read again, so that what another editor has written
	 * there in the meantime is not lost.
	 *
	 * An editor that is given no file remembers for as long as it exists and no longer. That is what a test or a
	 * tool wants; the settings of the person who is logged in (FEditorSettings::GetUserFile) are for the editor
	 * that this person works in.
	 *
	 * A file that cannot be read as settings, or cannot be written, is reported in the log as a warning. The
	 * editor then goes on with what it remembers, and one that could not be read is replaced when the settings
	 * next change.
	 *
	 * @param File The file, which need not exist yet. An empty path ends the keeping of settings in a file.
	 */
	EDITOR_API void SetSettingsFile(std::filesystem::path File);

	/**
	 * Returns the file that a path given to a command means: a relative path is relative to the content directory
	 * of the project, if one is open, so that a scene can be named as "Scenes/Level1.json". Any other path is
	 * returned as it is.
	 */
	EDITOR_API std::filesystem::path ResolveFilePath(const std::filesystem::path& Path) const;

	/**
	 * Returns the text that names a file to a command, and to the user: its path relative to the content
	 * directory of the project, with forward slashes, if the file is inside that directory, and its whole path
	 * otherwise. ResolveFilePath makes the file of it again.
	 */
	EDITOR_API std::string MakeProjectPath(const std::filesystem::path& File) const;

private:
	/** Returns the scene written down: everything that is saved with it, as the text of the history's states. */
	std::string WriteSceneState() const;

	/** Makes what has changed in the scene since the last step a step of the history. */
	void RecordChanges(std::string_view StepName);

	/** Replaces the scene with the one that a state of the history describes. */
	void RestoreSceneState(const std::string& State);

	/** Makes NewScene the scene being edited and destroys the one it replaces. */
	void ReplaceScene(WScene& NewScene);

	/** Reads the settings from their file, if they are kept in one and it is there. */
	void ReadSettings();

	/** Writes the settings to their file, if they are kept in one. */
	void WriteSettings() const;

	FCommandRegistry Commands;

	FUndoHistory History;

	/** The name of the step that BeginStep began and nothing has ended yet. */
	std::optional<std::string> OpenStepName;

	/** The scene being edited. It is in the root set, which is what keeps it alive; this is how to find it. */
	TWeakObjectPtr<WScene> Scene;

	std::filesystem::path ScenePath;

	/** The identifiers of the selected entities, some of which may no longer exist; see GetSelection. */
	std::vector<int64> SelectedEntityIds;

	FViewportCamera ViewportCamera;

	ESceneViewMode ViewMode = ESceneViewMode::Lit;

	std::optional<FProject> Project;

	FEditorSettings Settings;

	/** The file the settings are kept in; empty if they are kept nowhere. */
	std::filesystem::path SettingsFile;
};
