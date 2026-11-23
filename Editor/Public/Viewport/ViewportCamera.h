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
 *
 * The camera also knows what it looks at: its focus point, FocusDistance in front of it. That is the point the
 * camera goes around when it is orbited, and the distance at which panning moves the scene with the mouse.
 */
struct FViewportCamera
{
	/**
	 * The steepest the camera looks up or down, in degrees: just short of straight up, where turning left and right
	 * would stop meaning anything.
	 */
	static constexpr float MaxPitch = 89.0f;

	/** The nearest the focus point is to the camera, in metres: moving closer stops here, short of the point. */
	static constexpr double MinFocusDistance = 0.05;

	/** The slowest and the fastest the camera flies, in metres per second. */
	static constexpr float MinFlySpeed = 0.1f;
	static constexpr float MaxFlySpeed = 1000.0f;

	/**
	 * How much larger than a ball the distance is made from which Frame shows it, so that the ball does not touch
	 * the edges of the picture.
	 */
	static constexpr double FrameMargin = 1.25;

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
	 * How far in front of the camera the point is that the user is looking at, in metres. At least
	 * MinFocusDistance. A new scene's camera looks at about the origin.
	 */
	double FocusDistance = 8.0;

	/** How fast the camera flies, in metres per second. From MinFlySpeed to MaxFlySpeed. */
	float FlySpeed = 4.0f;

	/**
	 * Returns true if a picture can be drawn from this camera and it can be steered: every number is finite, the
	 * pitch is within MaxPitch, the field of view is one that FCameraView accepts, the exposure is greater than
	 * zero, and the focus distance and the fly speed are within their limits.
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

	/** Returns the point that the camera looks at: FocusDistance in front of it. */
	EDITOR_API FVector GetFocusPoint() const;

	/**
	 * Turns the camera to face a point, where it stands. The point becomes the focus point, or as near to it as
	 * the limits of the pitch and of the focus distance allow.
	 *
	 * @return False, leaving the camera as it was, if the point is where the camera is, or is not a finite position.
	 * A point straight above or below is faced as steeply as MaxPitch allows.
	 */
	EDITOR_API bool LookAt(const FVector& Target);

	/**
	 * Turns the camera where it stands, as a head is turned.
	 *
	 * @param YawDegrees How far to the left; negative turns to the right. The yaw stays from -180 to 180.
	 * @param PitchDegrees How far upwards; negative looks down. The pitch stops at MaxPitch either way.
	 */
	EDITOR_API void Turn(float YawDegrees, float PitchDegrees);

	/**
	 * Moves the camera without turning it, as flying does: sideways, up and ahead. The focus point moves along.
	 *
	 * @param Right Metres to the right of the picture; negative is to the left.
	 * @param Up Metres up along the world's up axis, whichever way the camera looks; negative is down.
	 * @param Forward Metres the way the camera looks; negative is backwards.
	 */
	EDITOR_API void Fly(double Right, double Up, double Forward);

	/**
	 * Takes the camera around its focus point, which stays where it is, in the scene and in the picture: the
	 * camera is turned as by Turn and then put where it faces the point from the same distance.
	 */
	EDITOR_API void Orbit(float YawDegrees, float PitchDegrees);

	/**
	 * Moves the camera towards its focus point, which stays where it is, or away from it.
	 *
	 * @param Distance Metres towards the point; negative moves away. The camera stops MinFocusDistance short of
	 * the point.
	 */
	EDITOR_API void Dolly(double Distance);

	/**
	 * Moves the camera in the plane of the picture, without turning it. The focus point moves along.
	 *
	 * @param Right Metres to the right of the picture.
	 * @param Up Metres towards the top of the picture, which is up only for a camera that looks level.
	 */
	EDITOR_API void Pan(double Right, double Up);

	/**
	 * Moves the camera, without turning it, to where it sees the whole of a ball, with some room around it; the
	 * middle of the ball becomes the focus point. This is how the camera is brought to look at something.
	 *
	 * @param Center The middle of the ball, a finite position.
	 * @param Radius The radius of the ball in metres, finite and greater than zero.
	 * @return False, leaving the camera as it was, if the ball is not one.
	 */
	EDITOR_API bool Frame(const FVector& Center, double Radius);

	/**
	 * Returns how many metres one pixel of a picture is at the focus point, for a picture of the given height.
	 * It is by how much the camera pans for the scene to follow the mouse.
	 *
	 * @param PictureHeight The height of the picture in pixels, greater than zero.
	 */
	EDITOR_API double GetPixelSizeAtFocus(int32 PictureHeight) const;

	/** Returns the view that the scene renderer draws a picture from. */
	EDITOR_API FCameraView GetView() const;

	bool operator==(const FViewportCamera& Other) const = default;
};
