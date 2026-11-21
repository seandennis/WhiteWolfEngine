#pragma once

#include "CoreMinimal.h"

#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

/**
 * The point of view a picture is drawn from.
 *
 * A camera looks along its own -Z axis, with +Y up and +X to the right, like every object in the engine's
 * right-handed, Y-up world.
 *
 * The matrices this type produces are what makes rendering independent of the distance from the world origin:
 * positions are made relative to the camera in double precision, and only then converted to the single precision
 * the graphics card works in. The view matrix therefore has no translation. Depth is "reversed": 1 at the near
 * plane, falling towards 0 at infinite distance, with no far plane, which keeps depth precise both close to the
 * camera and very far from it.
 */
struct FCameraView
{
	/** The smallest and largest vertical field of view, in degrees. */
	static constexpr float MinFieldOfView = 1.0f;
	static constexpr float MaxFieldOfView = 179.0f;

	/** Where the camera is, in world space. */
	FVector Location = FVector(0.0);

	/** Which way the camera faces. Must be a unit quaternion. The identity looks along -Z with +Y up. */
	FQuat Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f);

	/** The angle between the top and bottom edges of the picture, in degrees. */
	float VerticalFieldOfView = 60.0f;

	/** The distance from the camera, in metres, closer than which nothing is drawn. Must be greater than zero. */
	float NearPlane = 0.1f;

	/**
	 * Returns true if a picture can be drawn from this view: the location and rotation are finite numbers, the
	 * rotation has unit length, the field of view is between MinFieldOfView and MaxFieldOfView, and the near plane
	 * is greater than zero.
	 */
	ENGINE_API bool IsValid() const;

	/**
	 * Returns the matrix that places an object relative to the camera: it takes a position in the object's local
	 * space to an offset from the camera's location, still along the world's axes.
	 *
	 * The offset is calculated in double precision, so it is exact however far both are from the world origin.
	 *
	 * @param WorldTransform The object's transform in world space.
	 */
	ENGINE_API FMatrix4f GetCameraRelativeMatrix(const FTransform& WorldTransform) const;

	/**
	 * Returns the view matrix: it turns an offset from the camera along the world's axes into view space, where
	 * the camera looks along -Z. It has no translation.
	 */
	ENGINE_API FMatrix4f GetViewMatrix() const;

	/**
	 * Returns the projection matrix, from view space to clip space.
	 *
	 * In clip space, after the division by w, x runs from -1 at the left edge of the picture to 1 at the right, y
	 * from -1 at the bottom to 1 at the top, and depth from 1 at the near plane towards 0 at infinite distance.
	 *
	 * @param AspectRatio The width of the picture divided by its height. Must be greater than zero.
	 */
	ENGINE_API FMatrix4f GetProjectionMatrix(float AspectRatio) const;

	bool operator==(const FCameraView& Other) const = default;
};
