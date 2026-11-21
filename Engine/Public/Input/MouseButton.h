#pragma once

#include "CoreMinimal.h"

/** A button of the mouse. The values are the button codes of GLFW; see EKey. */
enum class EMouseButton : uint8
{
	Left = 0,
	Right = 1,
	Middle = 2,

	/** The first and second extra buttons, usually "back" and "forward" on the side of the mouse. */
	Extra1 = 3,
	Extra2 = 4,

	/** The largest button code; not a button itself. */
	Last = 7,
};
