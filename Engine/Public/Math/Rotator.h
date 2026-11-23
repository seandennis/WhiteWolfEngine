#pragma once

#include "Core/CoreTypes.h"
#include "Math/Quat.h"

/**
 * A rotation as three angles in degrees, which is how a person reads and writes one. Rotations are stored and
 * combined as quaternions (FQuat); a rotator is for showing a rotation and for making one from numbers.
 *
 * Each angle turns around one axis of the space the rotation is in, counter-clockwise as seen with the axis
 * pointing at the viewer:
 *
 *     Pitch   around +X, the right: a positive pitch tilts what faces forward (-Z) upwards
 *     Yaw     around +Y, up: a positive yaw turns what faces forward to the left
 *     Roll    around +Z, the back: a positive roll tips an object over onto its left side
 *
 * The roll is applied first, then the pitch, then the yaw, so the yaw always turns around the up axis, as a
 * turret does, and the roll around the way the object then faces.
 */
struct FRotator
{
	float Pitch = 0.0f;
	float Yaw = 0.0f;
	float Roll = 0.0f;

	/** Returns the rotation that the angles describe, as a unit quaternion. The angles may be of any size. */
	ENGINE_API FQuat ToQuat() const;

	/**
	 * Returns angles that describe a rotation: a pitch from -90 to 90, and a yaw and a roll from -180 to 180.
	 *
	 * Many sets of angles describe the same rotation, so the angles that a rotation was made from need not be the
	 * ones that come back; the rotation is the same. Straight up and straight down, yaw and roll turn around the
	 * same axis: within a tenth of a degree of vertical the pitch is 90 or -90, the roll is 0 and the yaw says
	 * what the two do together.
	 *
	 * @param Rotation A unit quaternion.
	 */
	ENGINE_API static FRotator FromQuat(const FQuat& Rotation);

	bool operator==(const FRotator& Other) const = default;
};
