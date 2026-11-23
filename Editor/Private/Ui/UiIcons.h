#pragma once

#include "CoreMinimal.h"

/**
 * Small square buttons that show a sign instead of a word, each as high as a line of widgets. The signs are drawn
 * with lines, in the colour of text, so they are sharp at every scale of the user interface.
 */
class FUiIcons
{
public:
	/**
	 * Lays out a button with an arrow that turns back on itself: put a value back as it was to begin with.
	 *
	 * @param Id What Dear ImGui knows the button by among the items around it, such as "##Reset".
	 * @return True if the button was clicked.
	 */
	static bool ResetButton(const char* Id);

	/** Lays out a button with a cross: take something away. */
	static bool RemoveButton(const char* Id);

	/** Lays out a button with a plus: add something. */
	static bool AddButton(const char* Id);

	/** Returns how wide and high each of the buttons is, in pixels. */
	static float GetButtonSize();
};
