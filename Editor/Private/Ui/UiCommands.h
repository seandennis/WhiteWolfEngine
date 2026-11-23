#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <vector>

class FEditor;
class FJsonValue;

/**
 * The commands that the widgets of the user interface give the editor.
 *
 * A widget does nothing to the scene itself: it gives the editor a command, the same one that an agent or a test
 * would give (D-040). What the user does as one thing is one step of the history, with a name that says what it
 * was (D-054); if the user was in the middle of something else, a change begun with BeginChange, that ends first.
 * A command that fails is reported by the editor in its log, which the user interface shows, so nothing here
 * returns whether it worked.
 */
class FUiCommands
{
public:
	/** Scene.New: replaces the scene being edited with an empty one. */
	static void NewScene(FEditor& Editor);

	/** Edit.Undo: undoes the last step of the history, if there is one. */
	static void Undo(FEditor& Editor);

	/** Edit.Redo: redoes the step that was undone last, if there is one. */
	static void Redo(FEditor& Editor);

	/** Selection.Set: selects the entities with the given identifiers and no others. */
	static void SetSelection(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Entity.Create: creates an entity and selects it.
	 *
	 * @param ParentId The entity it is attached to; at the root of the scene if not given.
	 * @return The identifier of the new entity, or nothing if the editor refused.
	 */
	static std::optional<int64> CreateEntity(FEditor& Editor, const std::string& Name, std::optional<int64> ParentId);

	/** Entity.Rename. */
	static void RenameEntity(FEditor& Editor, int64 EntityId, const std::string& Name);

	/**
	 * Entity.Delete for each of the entities, as one step. An entity that goes with one of the others, because it
	 * is below it, needs no command of its own.
	 */
	static void DeleteEntities(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Entity.Duplicate for each of the entities that is not below one of the others, as one step, and selects the
	 * copies.
	 */
	static void DuplicateEntities(FEditor& Editor, const std::vector<int64>& EntityIds);

	/**
	 * Entity.SetParent for each of the entities that is not below one of the others, as one step: they keep their
	 * places in the world.
	 *
	 * @param ParentId The new parent; the root of the scene if not given.
	 */
	static void AttachEntities(FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId);

	/**
	 * Returns true if AttachEntities can give the entities this parent: it is none of them and below none of them.
	 */
	static bool CanAttachEntities(
		const FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId);

	/** Viewport.Focus: brings the viewport's camera to where it sees the selected entities whole, if any are. */
	static void FocusSelection(FEditor& Editor);

	/** Component.Add: gives an entity a component of the named class. The step is named after the class. */
	static void AddComponent(FEditor& Editor, int64 EntityId, const std::string& ClassName);

	/** Component.Remove. The step is named after the component's class. */
	static void RemoveComponent(FEditor& Editor, int64 ComponentId);

	/**
	 * Begins a change that the user makes over several frames, such as dragging a number through many values: the
	 * commands given from now until EndChange are one step of the history, with this name.
	 */
	static void BeginChange(FEditor& Editor, std::string StepName);

	/** Ends the change that BeginChange began. Does nothing if none is under way. */
	static void EndChange(FEditor& Editor);

	/**
	 * Object.SetProperty: gives a property of an entity or component a value. During a change (BeginChange) it is
	 * part of that change, and a step by itself otherwise.
	 *
	 * @param Value The value, in the form the command takes it.
	 */
	static void SetProperty(FEditor& Editor, int64 ObjectId, const std::string& PropertyName, const FJsonValue& Value);
};
