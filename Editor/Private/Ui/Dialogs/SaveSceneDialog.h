#pragma once

#include "CoreMinimal.h"

#include <string>

class FEditor;
struct FUiFrame;

/**
 * The window that asks where to save the scene being edited: a field for the file, which with a project open is
 * a place in the project's content, and Save and Cancel. It has to be answered before anything else is done.
 *
 * A name without an ending is given the ending of scene files. If the scene cannot be saved there, the window
 * stays and says why.
 */
class FSaveSceneDialog
{
public:
	/**
	 * Has the window open when it is next laid out, with the scene's file in its field, or for a scene that has
	 * none yet a name made of the scene's.
	 *
	 * @param InError What to say went wrong, if the window opens because the scene could not be saved to its file.
	 */
	void Open(const FEditor& Editor, std::string InError = std::string());

	/** Lays the window out if it is open, and saves the scene if the user asks for it. */
	void Draw(FUiFrame& Frame);

private:
	/** True from Open until the window has been laid out. */
	bool bWantsToOpen = false;

	/** True until the field has been given the keyboard, which takes a frame after the window has opened. */
	bool bFocusField = false;

	/** What is in the field. */
	std::string PathText;

	/** Why the last attempt to save failed; empty if none has. */
	std::string Error;
};
