#pragma once

#include "CoreMinimal.h"

/** What the editor's main window should be like when it opens. */
struct FEditorWindowDescription
{
	/** The size of the window's content, in screen coordinates. Both must be positive. */
	int32 Width = 1600;
	int32 Height = 900;

	/**
	 * Whether every call to the rendering interface is checked, with mistakes reported as errors in the log. It
	 * costs time, so it is for tests and for finding a fault, not for everyday use.
	 */
	bool bValidateRendering = false;
};
