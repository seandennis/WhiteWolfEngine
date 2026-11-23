#pragma once

#include "CoreMinimal.h"

#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Viewport/GizmoHandle.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/TransformGizmo.h"

#include <optional>

struct FGizmoSettings;

/**
 * One drag of a handle of the gizmo: what it does to the things that the gizmo moves, from where the mouse took
 * hold of the handle to where the mouse is now.
 *
 * Everything is measured from the beginning of the drag: the gizmo as it lay then, and the place in the picture
 * where the mouse took hold. A thing is therefore put where the whole drag takes it from where it was, not moved
 * a little further with every movement of the mouse, and nothing adds up that should not.
 *
 * An arrow moves a thing along its axis, by as much as the mouse moves along the axis in the picture; a square,
 * in its plane, with the point that the mouse took hold of staying under the mouse. A ring turns a thing around
 * its axis and around the gizmo's place, by as much as the mouse moves along the ring from where it took hold: the
 * length of an axis is one radian. A box changes a thing's size along its axis, by the part of the axis' length
 * that the mouse moves along it, and the box in the middle changes it along all three axes, by the part of that
 * length that the mouse moves to the right and upwards. With steps switched on (FGizmoSettings), each of these
 * goes in whole steps.
 */
class FGizmoDrag
{
public:
	/**
	 * The least that a drag leaves of a thing's size along an axis, as a part of the size it had: a drag neither
	 * flattens a thing nor turns it inside out.
	 */
	static constexpr float MinScaleFactor = 0.01f;

	/**
	 * Begins a drag.
	 *
	 * @param InGizmo The gizmo as it lies when the mouse takes hold. The drag keeps a copy of it.
	 * @param InMode What the gizmo does.
	 * @param InHandle The handle that the mouse has taken hold of. None, a handle that the mode has not, or any
	 * handle of a gizmo that is not visible, is a drag that does nothing.
	 * @param InStartPoint Where in the picture the mouse took hold, in pixels from its top left corner.
	 */
	EDITOR_API FGizmoDrag(
		const FTransformGizmo& InGizmo, EGizmoMode InMode, EGizmoHandle InHandle, const FVector2f& InStartPoint);

	/**
	 * Takes the mouse to a point of the picture. Where the mouse then says nothing, the drag stays what it was:
	 * when it points along the very axis that it drags, when it points past the horizon of the plane that the drag
	 * is taken in, or at a place of that plane that is more than a hundred times as far away as the gizmo, and
	 * when the point is no number.
	 *
	 * @param PicturePoint Where the mouse is, in pixels from the top left corner of the picture.
	 * @param Settings Whether the drag goes in steps, and in which.
	 */
	EDITOR_API void MoveTo(const FVector2f& PicturePoint, const FGizmoSettings& Settings);

	/**
	 * Returns where the drag has taken a thing.
	 *
	 * @param StartTransform The transform that the thing had in the world when the drag began.
	 * @return The transform that it has in the world now.
	 */
	EDITOR_API FTransform Apply(const FTransform& StartTransform) const;

	/** Returns what the drag does: what the gizmo was set to do when the mouse took hold. */
	EGizmoMode GetMode() const
	{
		return Mode;
	}

	/** Returns the handle that is dragged; None for a drag of a gizmo that is not visible. */
	EGizmoHandle GetHandle() const
	{
		return Handle;
	}

	/** Returns how far the drag has moved things, in metres along the axes of the world. Zero unless it moves them. */
	const FVector& GetTranslation() const
	{
		return Translation;
	}

	/**
	 * Returns how far the drag has turned things around the axis of its handle, in degrees, counter-clockwise as
	 * seen from where the axis points. Zero unless it turns them.
	 */
	float GetAngle() const
	{
		return Angle;
	}

	/** Returns by how much the drag has multiplied the size of things along each of their axes. One unless it sizes
	 * them. */
	const FVector3f& GetScale() const
	{
		return Scale;
	}

private:
	/** Returns how far along the axis of the handle the mouse points, in metres from the gizmo's place. */
	std::optional<double> FindAlongAxis(const FVector2f& PicturePoint) const;

	/** Returns the point of the handle's plane that the mouse points at. */
	std::optional<FVector> FindInPlane(const FVector2f& PicturePoint) const;

	/**
	 * Returns the direction in the picture in which the ring of the handle runs where the mouse took hold of it,
	 * of unit length, the way that turns things counter-clockwise.
	 */
	FVector2f FindRingDirection() const;

	FTransformGizmo Gizmo;
	EGizmoMode Mode;
	EGizmoHandle Handle;
	FVector2f StartPoint;

	/** Where along the axis of the handle the mouse pointed when it took hold. */
	double StartAlongAxis = 0.0;

	/** The point of the handle's plane that it pointed at. */
	FVector StartInPlane = FVector(0.0);

	/** The direction of the ring where it took hold; see FindRingDirection. */
	FVector2f RingDirection = FVector2f(1.0f, 0.0f);

	FVector Translation = FVector(0.0);
	float Angle = 0.0f;
	FVector3f Scale = FVector3f(1.0f);
};
