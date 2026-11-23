#pragma once

#include "Core/CoreTypes.h"
#include "Math/Box.h"
#include "Math/Vector.h"

#include <optional>

/**
 * Where rays meet shapes, in a space of single precision such as that of a mesh.
 *
 * A ray is given by the point it begins at and a direction, which need not be of unit length: how far along the
 * ray something lies is counted in multiples of the direction. A ray that is taken into the space of a scaled
 * object, where its direction is longer or shorter than it was, therefore keeps its distances.
 */
class FIntersection
{
public:
	/**
	 * Returns where a ray meets the front of a triangle.
	 *
	 * @param Origin Where the ray begins.
	 * @param Direction Which way it goes; any length but zero.
	 * @param A One corner of the triangle. A, B and C run counter-clockwise as seen from the triangle's front.
	 * @return The multiple of Direction that leads from Origin to the point where they meet. Nothing if the ray
	 * misses the triangle, comes at it from behind or along its plane, or would only meet it behind where the ray
	 * begins, and nothing for a triangle without an area.
	 */
	ENGINE_API static std::optional<float> RayTriangle(const FVector3f& Origin,
		const FVector3f& Direction,
		const FVector3f& A,
		const FVector3f& B,
		const FVector3f& C);

	/**
	 * Returns true if a ray goes through a box, touches it or begins inside it. A box without thickness, such as
	 * that of a flat mesh, is a box like any other; an empty one is met by nothing.
	 *
	 * @param Origin Where the ray begins.
	 * @param Direction Which way it goes; any length but zero.
	 */
	ENGINE_API static bool RayBox(const FVector3f& Origin, const FVector3f& Direction, const FBox3f& Box);
};
