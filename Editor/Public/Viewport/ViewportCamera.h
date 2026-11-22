#pragma once

#include "CoreMinimal.h"

#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Rendering/CameraView.h"

/**
 * The point of view of the editor's viewport: where in the scene being edited the user looks from, and which way.
 *
 * The direction is two angles and not a rotation, because that is how a viewport is steered: turning left and right
 * and looking up and down, with the horizon always level.
 */
struct FViewportCamera
{
	/**
	 * The steepest the camera looks up or down, in degrees: just short of straight up, where turning left and right
	 * would stop meaning anything.
	 */
	static constexpr float MaxPitch = 89.0f;

	/** Where the camera is, in world space. A new scene is first seen from a short way off, looking at its origin. */
	FVector Location = FVector(4.25, 3.0, 6.0);

	/**
	 * How far the camera is turned around the world's up axis, in degrees, counter-clockwise when seen from above:
	 * at 0 it looks along -Z, at 90 along -X.
	 */
	float Yaw = 35.0f;

	/** How far the camera looks up from the horizon, in degrees; negative looks down. Within MaxPitch either way. */
	float Pitch = -22.0f;

	/** The angle between the top and bottom edges of the picture, in degrees. */
	float VerticalFieldOfView = 60.0f;

	/** How bright the picture is; see FCameraView::Exposure. */
	float Exposure = 1.0f;

	/**
	 * Returns true if a picture can be drawn from this camera: every number is finite, the pitch is within MaxPitch,
	 * the field of view is one that FCameraView accepts and the exposure is greater than zero.
	 */
	EDITOR_API bool IsValid() const;

	/** Returns the rotation that the two angles are. */
	EDITOR_API FQuat GetRotation() const;

	/** Returns the direction the camera looks in, as a unit vector. */
	EDITOR_API FVector3f GetForward() const;

	/** Returns the direction to the right of the picture, as a unit vector. It is always level. */
	EDITOR_API FVector3f GetRight() const;

	/** Returns the direction to the top of the picture, as a unit vector. */
	EDITOR_API FVector3f GetUp() const;

	/**
	 * Turns the camera to face a point, where it stands.
	 *
	 * @return False, leaving the camera as it was, if the point is where the camera is, or is not a finite position.
	 * A point straight above or below is faced as steeply as MaxPitch allows.
	 */
	EDITOR_API bool LookAt(const FVector& Target);

	/** Returns the view that the scene renderer draws a picture from. */
	EDITOR_API FCameraView GetView() const;

	bool operator==(const FViewportCamera& Other) const = default;
};
