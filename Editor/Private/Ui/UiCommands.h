#pragma once

#include "CoreMinimal.h"

#include <vector>

class FEditor;

/**
 * The commands that the widgets of the user interface give the editor.
 *
 * A widget does nothing to the scene itself: it gives the editor a command, the same one that an agent or a test
 * would give (D-040). A command that fails is reported by the editor in its log, which the user interface shows,
 * so nothing here returns a result.
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
};
