#pragma once

#include "CoreMinimal.h"

#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoSpace.h"

/**
 * How the gizmo of the viewport is set: what dragging it does, along which axes, and whether in steps.
 *
 * Like the viewport's camera, this is how the user works and not part of the scene. In steps, a thing moves, turns
 * or grows by whole multiples of the step from where the drag began, which is how things are lined up without
 * typing numbers.
 */
struct FGizmoSettings
{
	/** The shortest and the longest step of a move, in metres. */
	static constexpr double MinLocationSnap = 0.001;
	static constexpr double MaxLocationSnap = 10000.0;

	/** The smallest and the largest step of a turn, in degrees. */
	static constexpr float MinRotationSnap = 0.1f;
	static constexpr float MaxRotationSnap = 180.0f;

	/** The smallest and the largest step of a change in size: a part of the size that the thing had. */
	static constexpr float MinScaleSnap = 0.001f;
	static constexpr float MaxScaleSnap = 10.0f;

	/** What dragging the gizmo does. */
	EGizmoMode Mode = EGizmoMode::Translate;

	/** The axes it moves and turns things along. Sizes are always changed along a thing's own axes. */
	EGizmoSpace Space = EGizmoSpace::World;

	/** Whether a move goes in steps, and how long a step is, in metres. */
	bool bSnapLocation = false;
	double LocationSnap = 0.1;

	/** Whether a turn goes in steps, and how large a step is, in degrees. */
	bool bSnapRotation = false;
	float RotationSnap = 10.0f;

	/** Whether a change in size goes in steps, and how large a step is: a quarter is a quarter of the size. */
	bool bSnapScale = false;
	float ScaleSnap = 0.25f;

	/** Returns true if every step is a number within its limits. */
	EDITOR_API bool IsValid() const;

	/**
	 * Returns what dragging the gizmo does after a mode, when the modes are gone through in turn, as a key does
	 * that is pressed again and again: moving, turning, sizing, and moving again.
	 */
	EDITOR_API static EGizmoMode GetNextMode(EGizmoMode InMode);

	bool operator==(const FGizmoSettings& Other) const = default;
};
