#pragma once

#include "CoreMinimal.h"

/** What dragging the gizmo of the viewport does to what is selected. */
enum class EGizmoMode : uint8
{
	/** Moves it. */
	Translate,

	/** Turns it. */
	Rotate,

	/** Makes it larger or smaller. */
	Scale,
};
