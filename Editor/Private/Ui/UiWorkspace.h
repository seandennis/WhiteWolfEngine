#pragma once

#include "CoreMinimal.h"

/** How the user has arranged the editor's main window: which panels are open. */
struct FUiWorkspace
{
	bool bShowViewport = true;
	bool bShowOutliner = true;

	/**
	 * True when the panels are to be put where they are in the default layout, as they are when the editor starts
	 * and when the user asks for it. The layout is built at the start of the next frame.
	 */
	bool bWantsDefaultLayout = true;
};
