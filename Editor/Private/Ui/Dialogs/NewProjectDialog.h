#pragma once

#include "CoreMinimal.h"

#include <string>

class FEditor;
struct FUiFrame;

/**
 * The window that asks for a project to make: a field for its name, a field for the directory that the project's
 * own directory is made in, and Create and Cancel. It has to be answered before anything else is done.
 *
 * The project's directory is called like the project, and the window says where that puts it before it is made.
 * If the project cannot be made, the window stays and says why.
 */
class FNewProjectDialog
{
public:
	/**
	 * Has the window open when it is next laid out, with a name to type over and a place suggested: beside the
	 * project that was opened last, or for a first project where FProject::GetDefaultLocation says.
	 */
	void Open(const FEditor& Editor);

	/** Lays the window out if it is open, and makes the project if the user asks for it. */
	void Draw(FUiFrame& Frame);

private:
	/** True from Open until the window has been laid out. */
	bool bWantsToOpen = false;

	/** True until the field for the name has been given the keyboard, which takes a frame after the window opened. */
	bool bFocusField = false;

	/** What is in the field for the name. */
	std::string NameText;

	/** What is in the field for the directory that the project's own directory is made in. */
	std::string LocationText;

	/** Why the last attempt to make the project failed; empty if none has. */
	std::string Error;
};
