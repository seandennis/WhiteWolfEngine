#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandResult.h"
#include "Object/WeakObjectPtr.h"

#include <filesystem>
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
 * Create and destroy an editor on the main thread.
 */
class FEditor
{
public:
	/** The name of a new scene that was not given one. */
	static constexpr std::string_view DefaultSceneName = "Untitled";

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
	 * Carries out a command on this editor; see FCommandRegistry::Execute.
	 *
	 * @param Arguments A JSON object with one member per argument, or JSON null for no arguments.
	 */
	EDITOR_API FCommandResult ExecuteCommand(std::string_view Name, const FJsonValue& Arguments);

	/** Returns the scene being edited. */
	EDITOR_API WScene& GetScene() const;

	/**
	 * Makes another scene the one being edited, and destroys the scene it replaces with everything in it.
	 *
	 * @param NewScene A valid scene. The editor keeps it alive from now on. Only the editor may destroy it.
	 * @param NewScenePath The file NewScene was loaded from; empty for a scene that has no file yet.
	 */
	EDITOR_API void SetScene(WScene& NewScene, std::filesystem::path NewScenePath);

	/** Returns the file the scene was opened from or last saved to. Empty for a scene that has no file yet. */
	const std::filesystem::path& GetScenePath() const
	{
		return ScenePath;
	}

	/** Records the file the scene now belongs to, after it has been saved there. */
	EDITOR_API void SetScenePath(std::filesystem::path NewScenePath);

private:
	FCommandRegistry Commands;

	/** The scene being edited. It is in the root set, which is what keeps it alive; this is how to find it. */
	TWeakObjectPtr<WScene> Scene;

	std::filesystem::path ScenePath;
};
