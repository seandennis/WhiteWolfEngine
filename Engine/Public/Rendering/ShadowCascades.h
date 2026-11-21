#pragma once

#include "CoreMinimal.h"

#include "Math/Quat.h"
#include "Rendering/ShadowCascade.h"

#include <array>

struct FCameraView;

/**
 * The shadow maps of one directional light for one picture.
 *
 * A directional light's shadow is drawn from the light's point of view into shadow maps. One map for the whole
 * view would be far too coarse near the camera, so the view is cut into slices by distance and each slice gets a
 * map of its own, a cascade: small and sharp near the camera, large and coarse far away.
 *
 * Each cascade covers a sphere around its slice of the view, so its size does not change when the camera turns,
 * and it moves in whole pixels of its shadow map as the camera moves, measured in the world, in double precision.
 * Together these keep the edges of shadows from crawling when the camera moves or turns.
 */
struct FShadowCascades
{
	/** How many cascades a light has. */
	static constexpr int32 CascadeCount = 4;

	/** How much farther towards the light than the view reaches things still cast shadows, in shadow distances. */
	static constexpr float CasterDistanceFactor = 4.0f;

	/** The cascades, nearest the camera first. */
	std::array<FShadowCascade, CascadeCount> Cascades;

	/**
	 * Works out the cascades of a light.
	 *
	 * @param View The camera. It must be valid.
	 * @param AspectRatio The width of the picture divided by its height. Must be greater than zero.
	 * @param LightRotation The rotation of the light; it shines along its -Z axis. A unit quaternion.
	 * @param ShadowDistance How far from the camera shadows reach, in metres. Must be greater than zero.
	 * @param ShadowMapSize The width and height of each shadow map, in pixels. Must be greater than zero.
	 */
	ENGINE_API static FShadowCascades Compute(const FCameraView& View,
		float AspectRatio,
		const FQuat& LightRotation,
		float ShadowDistance,
		int32 ShadowMapSize);
};
