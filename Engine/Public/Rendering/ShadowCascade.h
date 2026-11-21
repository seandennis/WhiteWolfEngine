#pragma once

#include "CoreMinimal.h"

#include "Math/Matrix.h"

/**
 * One of the shadow maps of a directional light: the part of the view it covers and how positions map into it.
 * See FShadowCascades.
 */
struct FShadowCascade
{
	/**
	 * Takes a camera-relative position into the shadow map: x and y from -1 to 1 across the map, and a depth from
	 * 0, nearest the light, to 1, farthest from it.
	 */
	FMatrix4f RelativeToShadow = FMatrix4f(1.0f);

	/** The distance from the camera, along the way it looks, up to which this cascade is used, in metres. */
	float FarDistance = 0.0f;

	/** The size in the world of one pixel of the shadow map, in metres. */
	float TexelSize = 0.0f;

	/** The distance along the light's direction that the depth from 0 to 1 stands for, in metres. */
	float DepthRange = 0.0f;
};
