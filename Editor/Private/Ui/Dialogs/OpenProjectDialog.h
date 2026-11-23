#pragma once

#include "CoreMinimal.h"

#include "Project/Project.h"

#include <string>
#include <vector>

class FEditor;
struct FUiFrame;

/**
 * The window that asks which project to work on: the projects that were opened last to choose from, a field for
 * the project file or the directory of any other, and Open, New Project and Cancel. It has to be answered before
 * anything else is done.
 *
 * A click on a project of the list puts its file into the field and a double click opens it. If the project
 * cannot be opened, the window stays and says why. New Project closes the window and asks for the one in which a
 * project is made (FUiRequests).
 */
class FOpenProjectDialog
{
public:
	/** Has the window open when it is next laid out, with the recent projects that can be opened now. */
	void Open(const FEditor& Editor);

	/** Lays the window out if it is open, and opens the project that the user asks for. */
	void Draw(FUiFrame& Frame);

private:
	/** True from Open until the window has been laid out. */
	bool bWantsToOpen = false;

	/** True until the field has been given the keyboard, which takes a frame after the window has opened. */
	bool bFocusField = false;

	/** The projects to choose from, as they were when the window was opened: the one opened last first. */
	std::vector<FProject> RecentProjects;

	/** What is in the field. */
	std::string PathText;

	/** Why the last attempt to open failed; empty if none has. */
	std::string Error;
};
