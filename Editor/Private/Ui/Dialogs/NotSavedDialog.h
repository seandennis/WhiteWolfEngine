#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

struct FUiFrame;

/**
 * The window that says what could not be written to its file when everything that was changed was saved: one line
 * for each, with the reason, and Close, which Enter and Escape press too. It has to be answered before anything else
 * is done, so that a material that could not be saved does not look saved.
 */
class FNotSavedDialog
{
public:
	/**
	 * Has the window open when it is next laid out.
	 *
	 * @param InProblems The sentences of the commands that could not save, one for each thing that is not saved.
	 */
	void Open(std::vector<std::string> InProblems);

	/**
	 * Lays the window out if it is open.
	 *
	 * @return True in the frame in which the user closes the window.
	 */
	bool Draw(FUiFrame& Frame);

private:
	/** True from Open until the window has been laid out. */
	bool bWantsToOpen = false;

	/** What could not be saved, and why. */
	std::vector<std::string> Problems;
};
