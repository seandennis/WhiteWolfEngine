#pragma once

#include "CoreMinimal.h"

/**
 * A part of the gizmo of the viewport that the mouse takes hold of. Which of them there are depends on what the
 * gizmo does (EGizmoMode): an arrow along each axis and a square between each two to move a thing, a ring around
 * each axis to turn it, and a box on each axis and one in the middle to change its size.
 */
enum class EGizmoHandle : uint8
{
	/** No handle. */
	None,

	/** Along the X axis, or around it. */
	X,

	/** Along the Y axis, or around it. */
	Y,

	/** Along the Z axis, or around it. */
	Z,

	/** In the plane of the Y and the Z axis: along both at once. */
	YZ,

	/** In the plane of the X and the Z axis. */
	XZ,

	/** In the plane of the X and the Y axis. */
	XY,

	/** Along all three axes alike: the size of a thing as a whole. */
	All,
};
