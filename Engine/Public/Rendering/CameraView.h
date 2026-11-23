#pragma once

#include "CoreMinimal.h"

#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Ray.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

#include <optional>

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
	 * How bright the picture is: the light of the scene is multiplied by this before it is fitted to the display.
	 * Twice the exposure is one photographic stop brighter. Must be greater than zero. A scene lit with real-world
	 * amounts of light, such as a hundred thousand lux of sunlight, needs a correspondingly small exposure.
	 */
	float Exposure = 1.0f;

	/**
	 * Returns true if a picture can be drawn from this view: the location and rotation are finite numbers, the
	 * rotation has unit length, the field of view is between MinFieldOfView and MaxFieldOfView, and the near plane
	 * and the exposure are finite and greater than zero.
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

	/**
	 * Returns the ray along which the camera sees a point of its picture: whatever is drawn at that point lies on
	 * it. It begins where the picture does, on the near plane, and goes away from the camera.
	 *
	 * @param PicturePoint The point, as the projection counts: x from -1 at the left edge of the picture to 1 at
	 * the right, y from -1 at the bottom to 1 at the top. A point beyond the edges gives the ray through where the
	 * picture would go on.
	 * @param AspectRatio The width of the picture divided by its height. Must be greater than zero.
	 */
	ENGINE_API FRay GetRay(const FVector2f& PicturePoint, float AspectRatio) const;

	/**
	 * Returns where in the picture a point of the world is drawn: the opposite of GetRay. Whoever draws something
	 * over a picture of a scene, as an editor draws its handles, finds the place for it with this.
	 *
	 * @param WorldPoint The point, in world space.
	 * @param AspectRatio The width of the picture divided by its height. Must be greater than zero.
	 * @return The point as the projection counts, x from -1 at the left edge of the picture to 1 at the right and
	 * y from -1 at the bottom to 1 at the top, and beyond those for a point that is to the side of what the
	 * picture shows. Nothing for a point that is closer to the camera than the near plane, or behind the camera:
	 * such a point is drawn nowhere.
	 */
	ENGINE_API std::optional<FVector2f> Project(const FVector& WorldPoint, float AspectRatio) const;

	bool operator==(const FCameraView& Other) const = default;
};
