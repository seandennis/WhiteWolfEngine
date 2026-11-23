#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

class FEditor;
struct FUiFrame;

/**
 * The window that asks which scene to open: the scene files of the project to choose from, a field for the file,
 * and Open and Cancel. It has to be answered before anything else is done.
 *
 * A click on a scene of the list puts it into the field and a double click opens it. If the file cannot be
 * opened, the window stays and says why.
 */
class FOpenSceneDialog
{
public:
	/** Has the window open when it is next laid out, with the project's scenes as they are now. */
	void Open(FEditor& Editor);

	/** Lays the window out if it is open, and opens the scene that the user asks for. */
	void Draw(FUiFrame& Frame);

private:
	/** True from Open until the window has been laid out. */
	bool bWantsToOpen = false;

	/** True until the field has been given the keyboard, which takes a frame after the window has opened. */
	bool bFocusField = false;

	/** The scene files of the project, as they were when the window was opened. */
	std::vector<std::string> Scenes;

	/** What is in the field. */
	std::string PathText;

	/** Why the last attempt to open failed; empty if none has. */
	std::string Error;
};
