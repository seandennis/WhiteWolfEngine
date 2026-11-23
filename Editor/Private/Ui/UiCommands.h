#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <vector>

class FEditor;

/**
 * The commands that the widgets of the user interface give the editor.
 *
 * A widget does nothing to the scene itself: it gives the editor a command, the same one that an agent or a test
 * would give (D-040). What the user does as one thing is one step of the history, with a name that says what it
 * was (D-054). A command that fails is reported by the editor in its log, which the user interface shows, so
 * nothing here returns whether it worked.
 */
class FUiCommands
{
public:
	/** Scene.New: replaces the scene with an empty one. */
	static void NewScene(FEditor& Editor);

	/** Edit.Undo. */
	static void Undo(FEditor& Editor);

	/** Edit.Redo. */
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
};
