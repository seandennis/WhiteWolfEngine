#pragma once

#include "CoreMinimal.h"

#include "Math/Vector.h"
#include "Viewport/GizmoHandle.h"

#include <vector>

/**
 * A line or an area of the picture that is part of a handle of the gizmo: what is drawn for the handle, and what
 * the mouse has to be on, or near, to take hold of it.
 */
struct FGizmoShape
{
	/** The handle that the shape is part of. */
	EGizmoHandle Handle = EGizmoHandle::None;

	/** True for an area, which is filled; false for a line. */
	bool bIsArea = false;

	/**
	 * The corners of the area, in order around it, or the points that the line goes through from one end to the
	 * other, in pixels from the top left corner of the picture. An area bulges nowhere inwards.
	 */
	std::vector<FVector2f> Points;

	/**
	 * Returns how far a point of the picture is from the shape, in pixels: from the nearest point of a line, or
	 * from the nearest edge of an area, and zero inside an area. An area whose corners are all in one line, or in
	 * one place, has no inside. Infinitely far from a shape without points.
	 */
	EDITOR_API float GetDistanceTo(const FVector2f& Point) const;
};
