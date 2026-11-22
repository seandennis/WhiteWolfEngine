#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandResult.h"
#include "History/UndoHistory.h"
#include "Object/WeakObjectPtr.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

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

private:
	/** Returns the scene written down: everything that is saved with it, as the text of the history's states. */
	std::string WriteSceneState() const;

	/** Makes what has changed in the scene since the last step a step of the history. */
	void RecordChanges(std::string_view StepName);

	/** Replaces the scene with the one that a state of the history describes. */
	void RestoreSceneState(const std::string& State);

	/** Makes NewScene the scene being edited and destroys the one it replaces. */
	void ReplaceScene(WScene& NewScene);

	FCommandRegistry Commands;

	FUndoHistory History;

	/** The scene being edited. It is in the root set, which is what keeps it alive; this is how to find it. */
	TWeakObjectPtr<WScene> Scene;

	std::filesystem::path ScenePath;
};
