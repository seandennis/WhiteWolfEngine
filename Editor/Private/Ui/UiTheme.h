#pragma once

#include "CoreMinimal.h"

#include "Math/Color.h"
#include "Math/LinearColor.h"

/**
 * How the editor looks: dark grey panels, light grey text, a blue accent for what is selected in a panel and an
 * orange line around what is selected in the viewport, in the manner of the editors its users know.
 */
class FUiTheme
{
public:
	/** Gives the current Dear ImGui context the editor's font. Call it once per context. */
	static void AddFonts();

	/**
	 * Gives the current Dear ImGui context the editor's colours and sizes.
	 *
	 * @param Scale By how much every size is multiplied: 1 on a display of the usual pixel density. Letters are
	 * drawn anew for the size they come out at, not stretched.
	 */
	static void Apply(float Scale);

	/**
	 * Returns the colour that marks one of the numbers of a value that has several: red, green and blue for the
	 * X, Y and Z of a vector or of a rotation and for the red, green and blue of a colour, and grey for a fourth
	 * number, such as alpha.
	 *
	 * @param PartIndex Which of the numbers, from 0.
	 */
	static FColor GetPartColor(int32 PartIndex);

	/** Returns the colour of text that says that something went wrong. */
	static FColor GetErrorColor();

	/**
	 * Returns the colour of the line that the viewport draws around what is selected: an orange, which stands out
	 * from most of what a scene shows. As an amount of light, which is how the renderer takes colours.
	 */
	static FLinearColor GetSelectionOutlineColor();

	/**
	 * Returns how wide that line is, in pixels of the surface: a size like the others of the user interface, which
	 * is larger at a larger scale.
	 */
	static float GetSelectionOutlineWidth();

	/**
	 * Makes the headings that are laid out next bands of grey: a heading that opens and closes what is under it
	 * is not something that is selected, which is what the accent means. PopHeadingColors follows the headings.
	 */
	static void PushHeadingColors();

	/** Ends what PushHeadingColors began. */
	static void PopHeadingColors();
};
