#pragma once

#include "CoreMinimal.h"

#include <string>

/**
 * What the editor's windows that have to be answered have in common: they open in the middle of the main window,
 * are as large as what they show, and say what went wrong in the same way.
 */
class FUiDialog
{
public:
	/**
	 * Begins a window that has to be answered before anything else is done. It is open once ImGui::OpenPopup has
	 * been called with the same title. What the window shows follows if this returns true, and then
	 * ImGui::EndPopup.
	 *
	 * @param Title What the window is called. Dear ImGui also knows the window by this.
	 */
	static bool Begin(const char* Title);

	/** Lays out a line that says what went wrong, in the colour for that. Nothing if there is nothing to say. */
	static void ShowError(const std::string& Error);

	/** Returns how wide the fields and lists of such a window are, in pixels: room for a path. */
	static float GetFieldWidth();

	/**
	 * Returns a path as it is shown where it has only so many pixels to itself, in the letters that are being
	 * drawn with: whole if it fits, and otherwise its end, which says the most about it (FUiText::FitEnd).
	 */
	static std::string FitPath(const std::string& Path, float Width);
};
