#pragma once

#include "CoreMinimal.h"

/**
 * How the editor looks: dark grey panels, light grey text and a blue accent for what is selected, in the manner of
 * the editors its users know.
 */
class FUiTheme
{
public:
	/** Gives the current Dear ImGui context the editor's colours, spacing and font. Call it once per context. */
	static void Apply();
};
