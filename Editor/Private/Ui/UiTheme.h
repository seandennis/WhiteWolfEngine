#pragma once

#include "CoreMinimal.h"

/**
 * How the editor looks: dark grey panels, light grey text and a blue accent for what is selected, in the manner of
 * the editors its users know.
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
};
