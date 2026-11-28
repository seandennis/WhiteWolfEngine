#pragma once

#include "CoreMinimal.h"

#include "Ui/ImGuiLibrary.h"

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
	 * Enter or Escape that is down when the window appears was pressed for what was there before: nothing in the
	 * window reads it until it is let go, neither as an answer nor in a field, however long it is held.
	 *
	 * @param Title What the window is called. Dear ImGui also knows the window by this.
	 */
	static bool Begin(const char* Title);

	/**
	 * Returns true if a key that answers the window being laid out, as Escape cancels it, went down while the window
	 * was there to be seen, in a window begun with Begin. Never for a key that was down when the window appeared,
	 * which was pressed for what was there before, as the Escape that closes one window, which may open another in
	 * that same frame, or the Enter that chooses the entry of a menu that opens one; and once for a key that is
	 * held, however long it repeats.
	 *
	 * @param Key A key, which may be pressed while it is being typed into a field of the window.
	 */
	static bool IsAnswerKeyPressed(ImGuiKey Key);

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
