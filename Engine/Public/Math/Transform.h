#pragma once

#include "Core/CoreTypes.h"
#include "Math/Quat.h"
#include "Math/Vector.h"

/**
 * A location, rotation and scale.
 *
 * A transform maps a point from its own local space to its parent space by scaling, then rotating, then translating
 * it. The location is double precision so that a transform can sit anywhere in a very large world; rotation and
 * scale are single precision. A default-constructed transform is the identity.
 *
 * A transform cannot represent shear. Combining transforms that have both rotation and non-uniform scale is
 * therefore approximate in the same way as in other engines that store location, rotation and scale separately;
 * combinations with uniform scale are exact up to rounding.
 */
struct FTransform
{
	/** Where the origin of the local space is, in the parent space. */
	FVector Location = FVector(0.0);

	/** The orientation of the local space in the parent space. Must be a unit quaternion. */
	FQuat Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f);

	/** The size of the local space's units along each local axis, in parent-space units. */
	FVector3f Scale = FVector3f(1.0f);

	/** Maps a position from local space to parent space. */
	ENGINE_API FVector TransformPosition(const FVector& Position) const;

	/**
	 * Maps a position from parent space to local space; the inverse of TransformPosition.
	 *
	 * A scale component that is zero cannot be inverted; the corresponding local coordinate of the result is zero.
	 */
	ENGINE_API FVector InverseTransformPosition(const FVector& Position) const;

	/**
	 * Maps a direction, or the difference of two positions, from parent space to local space: it is turned and
	 * scaled as InverseTransformPosition turns and scales a position, and not moved. A direction of unit length is
	 * in general not of unit length afterwards.
	 *
	 * A scale component that is zero cannot be inverted; the corresponding local coordinate of the result is zero.
	 */
	ENGINE_API FVector3f InverseTransformVector(const FVector3f& Vector) const;

	/**
	 * Returns true if what this transform does can be undone: no component of its scale is zero, or so close to
	 * zero that it cannot be divided by. InverseTransformPosition and InverseTransformVector then give back what
	 * went into the transform, and otherwise flatten it.
	 */
	ENGINE_API bool IsInvertible() const;

	/**
	 * Returns the transform that applies Child and then Parent.
	 *
	 * If Child places an object in Parent's local space, the result places that object in Parent's parent space.
	 */
	ENGINE_API static FTransform Combine(const FTransform& Parent, const FTransform& Child);

	/**
	 * Returns this transform expressed in the local space of Parent; the inverse of Combine.
	 *
	 * Combine(Parent, Transform.GetRelativeTransform(Parent)) gives back Transform, exactly up to rounding when
	 * Parent's scale is uniform. A zero component in Parent's scale yields zero for that axis.
	 */
	ENGINE_API FTransform GetRelativeTransform(const FTransform& Parent) const;

	bool operator==(const FTransform& Other) const = default;
};
