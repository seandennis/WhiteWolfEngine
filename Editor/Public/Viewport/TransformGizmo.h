#pragma once

#include "CoreMinimal.h"

#include "Math/Quat.h"
#include "Math/Ray.h"
#include "Math/Vector.h"
#include "Rendering/CameraView.h"
#include "Viewport/GizmoHandle.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoShape.h"

#include <array>
#include <optional>
#include <vector>

/**
 * The gizmo of the viewport as it lies in one picture: the handles with which the user moves, turns and sizes what
 * is selected, at a place in the world and seen from a camera.
 *
 * This is geometry and nothing else. It says where in the picture the handles are, as lines and areas that
 * whoever shows the picture draws over it, and which handle is at a point of the picture. What happens when a
 * handle is dragged is FGizmoDrag's to say.
 *
 * The gizmo is the same size in the picture wherever it is in the world: its axes are so many pixels long, and as
 * many metres as that is at its distance from the camera. Points of the picture are in pixels from its top left
 * corner, as a mouse has them.
 */
class FTransformGizmo
{
public:
	/** How many straight pieces a whole ring of the gizmo is made of. */
	static constexpr int32 RingPieceCount = 64;

	/**
	 * Lays the gizmo out for a picture.
	 *
	 * @param InView From where the picture is taken.
	 * @param InPictureSize The width and the height of the picture, in pixels; both greater than zero.
	 * @param InPivot Where the gizmo is: the place in the world of the thing that it moves.
	 * @param Orientation The rotation of the gizmo's axes; none for the axes of the world. It need not be of unit
	 * length: only the way it turns counts, and a rotation of no length turns nothing.
	 * @param InAxisLength How long its axes are in the picture, in pixels; greater than zero.
	 */
	EDITOR_API FTransformGizmo(const FCameraView& InView,
		const FVector2f& InPictureSize,
		const FVector& InPivot,
		const FQuat& Orientation,
		float InAxisLength);

	/**
	 * Returns true if the gizmo is somewhere that a picture shows: in front of the camera, beyond its near plane.
	 * A gizmo that is not has no shapes and no handles.
	 */
	bool IsVisible() const
	{
		return bIsVisible;
	}

	/** Returns where the gizmo is, in world space. */
	const FVector& GetPivot() const
	{
		return Pivot;
	}

	/**
	 * Returns the direction of one of the gizmo's axes in the world, of unit length.
	 *
	 * @param AxisIndex 0 for X, 1 for Y, 2 for Z. Anything else is a fatal error.
	 */
	EDITOR_API const FVector3f& GetAxis(int32 AxisIndex) const;

	/** Returns how long the gizmo's axes are in the picture, in pixels. */
	float GetAxisLength() const
	{
		return AxisLength;
	}

	/** Returns how long the gizmo's axes are in the world, in metres: their length in the picture, at its distance. */
	double GetWorldAxisLength() const
	{
		return WorldAxisLength;
	}

	/** Returns the direction from the camera to the gizmo, of unit length: the line of sight to it. */
	const FVector3f& GetViewDirection() const
	{
		return ViewDirection;
	}

	/** Returns the ray along which the camera sees a point of the picture. */
	EDITOR_API FRay GetRay(const FVector2f& PicturePoint) const;

	/**
	 * Returns where in the picture a point of the world is, or nothing if it is not in front of the camera's near
	 * plane.
	 */
	EDITOR_API std::optional<FVector2f> FindInPicture(const FVector& WorldPoint) const;

	/**
	 * Returns the shapes of the handles that the gizmo has in a mode. A handle that cannot be used from where the
	 * camera is has none: an arrow that points at the camera or away from it, which no movement of the mouse
	 * would move along, and a square that is seen from its edge.
	 *
	 * Of a ring, the part is there that is not behind the middle of the gizmo as the camera sees it: the half
	 * that is towards the camera, and all of a ring that is seen squarely. Rings that lean different ways can
	 * then be told apart, and where the mouse is on a ring, it is on the side that is seen.
	 */
	EDITOR_API std::vector<FGizmoShape> GetShapes(EGizmoMode Mode) const;

	/**
	 * Returns the handle at a point of the picture: the one with the shape that is nearest to the point, if that
	 * is no farther from it than Reach, in pixels. A point inside an area is on it, and nearer to it than to any
	 * line.
	 */
	EDITOR_API EGizmoHandle FindHandle(EGizmoMode Mode, const FVector2f& PicturePoint, float Reach) const;

	/**
	 * Returns a point of the picture at which the mouse has a handle, for whoever takes hold of one without
	 * looking for it, as a test does: halfway along the head of an arrow, in the middle of a square or a box, and
	 * on a ring halfway between the two axes that it lies in, at the first of the four such points, counted
	 * around the ring from the first of those axes, at which the mouse has the ring.
	 *
	 * @return Nothing if the mode has no such handle, if the handle cannot be used from where the camera is, or if
	 * another handle lies over that point of it.
	 */
	EDITOR_API std::optional<FVector2f> FindHandlePoint(EGizmoMode Mode, EGizmoHandle Handle) const;

	/**
	 * Returns the index of the axis that a handle is along or around, or, for the handle of a plane, of the axis
	 * that the plane leaves out: 0 for X and YZ, 1 for Y and XZ, 2 for Z and XY. Nothing for None and All.
	 */
	EDITOR_API static std::optional<int32> GetAxisIndex(EGizmoHandle Handle);

	/** Returns true for the handles that are along one axis, or around it: X, Y and Z. */
	static bool IsAxisHandle(EGizmoHandle Handle)
	{
		return Handle == EGizmoHandle::X || Handle == EGizmoHandle::Y || Handle == EGizmoHandle::Z;
	}

	/** Returns true for the handles that are in the plane of two axes: YZ, XZ and XY. */
	static bool IsPlaneHandle(EGizmoHandle Handle)
	{
		return Handle == EGizmoHandle::YZ || Handle == EGizmoHandle::XZ || Handle == EGizmoHandle::XY;
	}

	/**
	 * Returns the direction from the middle of the gizmo to the point at which a piece of the ring around an axis
	 * begins, of unit length. Pieces are counted around the ring the way that a thing turns counter-clockwise as
	 * seen from where the axis points, beginning at the next of the gizmo's axes, and on from the last piece to
	 * the first.
	 *
	 * @param AxisIndex 0 for X, 1 for Y, 2 for Z. Anything else is a fatal error.
	 */
	EDITOR_API FVector3f GetRingPointDirection(int32 AxisIndex, int32 Piece) const;

	/**
	 * Returns true if the point at which a piece of the ring around an axis begins is not behind the middle of
	 * the gizmo, as the camera sees it. A ring is drawn through these points and through no others, so they are
	 * also the only ones that the mouse can have taken hold of.
	 *
	 * @param AxisIndex 0 for X, 1 for Y, 2 for Z. Anything else is a fatal error.
	 */
	EDITOR_API bool IsRingPointInFront(int32 AxisIndex, int32 Piece) const;

private:
	/** Returns true if the arrow or the box of an axis can be used from where the camera is. */
	bool IsAxisUsable(int32 AxisIndex) const;

	/** Returns true if the square of the plane that leaves an axis out can be used from where the camera is. */
	bool IsPlaneUsable(int32 AxisIndex) const;

	/** Returns the point of the world that is so many axis lengths along each of the gizmo's axes from its pivot. */
	FVector GetPoint(const FVector3f& AxisLengths) const;

	/** Adds the shapes of the handles that move things: an arrow along each axis and a square in each plane. */
	void AddTranslateShapes(std::vector<FGizmoShape>& OutShapes) const;

	/** Adds the shapes of the handles that turn things: a ring around each axis. */
	void AddRotateShapes(std::vector<FGizmoShape>& OutShapes) const;

	/** Adds the shapes of the handles that size things: a box at the end of each axis and one in the middle. */
	void AddScaleShapes(std::vector<FGizmoShape>& OutShapes) const;

	FCameraView View;
	FVector2f PictureSize;
	FVector Pivot;
	std::array<FVector3f, 3> Axes;
	float AxisLength;
	double WorldAxisLength = 0.0;

	/** The direction from the camera to the pivot, of unit length. */
	FVector3f ViewDirection = FVector3f(0.0f, 0.0f, -1.0f);

	bool bIsVisible = false;
};
