#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

/**
 * A ray in the world: a line that begins at a point and goes on in one direction without end. What a camera sees
 * at a point of its picture lies along a ray, and so does what a pointer points at.
 *
 * The origin is a position in world space, in double precision. The direction is of unit length, so that a
 * distance along the ray is in metres.
 */
struct FRay
{
	/** Where the ray begins. */
	FVector Origin = FVector(0.0);

	/** Which way the ray goes. Must be of unit length. */
	FVector3f Direction = FVector3f(0.0f, 0.0f, -1.0f);

	/** Returns the point that lies a distance, in metres, along the ray from where it begins. */
	FVector GetPoint(double Distance) const
	{
		return Origin + FVector(Direction) * Distance;
	}

	bool operator==(const FRay& Other) const = default;
};
