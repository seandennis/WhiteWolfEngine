#pragma once

#include "CoreMinimal.h"

#include <optional>

/**
 * What one part of the user interface asks of another. Whoever is asked looks at the request when it is next laid
 * out, and takes it away.
 */
struct FUiRequests
{
	/** The entity whose row in the outliner is to turn into a text field for its name. */
	std::optional<int64> EntityToRename;

	/**
	 * True if what was changed is to be saved, as the Save button of the toolbar asks: the materials that were
	 * changed, and then the scene, to its file or, if it has none yet, to one that the user is then asked for. The
	 * menu bar, which has the windows that ask, does it.
	 */
	bool bSave = false;

	/**
	 * True if the user is to be asked which project to work on, as the editor asks when it starts. The menu bar
	 * opens the window for it.
	 */
	bool bOpenProject = false;

	/**
	 * True if the user is to be asked for the name and the place of a project to make, as the window that opens
	 * projects asks for someone who has none yet. The menu bar opens the window for it.
	 */
	bool bNewProject = false;
};
