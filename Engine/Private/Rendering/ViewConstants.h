#pragma once

#include "CoreMinimal.h"

#include "Math/Matrix.h"
#include "Rendering/DirectionalLightData.h"
#include "Rendering/PointLightData.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/ShadowCascades.h"

#include <glm/vec4.hpp>

#include <array>

/**
 * What is the same for every mesh of one picture, as the shaders receive it: the lights.
 * Must match FViewConstants in Engine/Shaders/SceneData.glsl, member for member, including the array sizes.
 */
struct FViewConstants
{
	/** x: the number of directional lights, y: the number of point lights. */
	glm::ivec4 LightCounts = glm::ivec4(0);

	std::array<FDirectionalLightData, FSceneRenderer::MaxDirectionalLights> DirectionalLights;
	std::array<FPointLightData, FSceneRenderer::MaxPointLights> PointLights;

	/**
	 * The light a matte surface gathers from the sky light's environment, as the nine coefficients of
	 * FSphericalHarmonics in rgb, already multiplied by the sky light's intensity.
	 */
	std::array<glm::vec4, 9> SkyIrradiance{};

	/** Turns a camera-relative direction into the direction in the environment's picture. */
	FMatrix4f RelativeToEnvironment = FMatrix4f(1.0f);

	/** Gives the camera-relative direction the camera looks in at a place on the screen. */
	FMatrix4f ClipToDirection = FMatrix4f(1.0f);

	/**
	 * x: the intensity of the sky light, zero if the scene has none. y: the number of the last, roughest level of
	 * the environment's reflection cube map. w: the level of detail the sky is read at.
	 */
	glm::vec4 SkyParameters = glm::vec4(0.0f);

	/** xyz: the way the camera looks, a unit vector. w: how far from the camera shadows reach. */
	glm::vec4 CameraForward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

	/** The distance from the camera, along the way it looks, at which each shadow cascade ends. */
	glm::vec4 ShadowCascadeFarDistances = glm::vec4(0.0f);

	/**
	 * For each cascade, x: the size of one pixel of its shadow maps in the world, y: that size as a part of the
	 * depth the maps cover.
	 */
	std::array<glm::vec4, FShadowCascades::CascadeCount> ShadowCascadeScales{};

	/** x: one divided by the width of a shadow map in pixels. */
	glm::vec4 ShadowParameters = glm::vec4(0.0f);

	/** Takes a camera-relative position into each shadow map. A light's maps follow each other, one per cascade. */
	std::array<FMatrix4f, FSceneRenderer::MaxDirectionalLights * FShadowCascades::CascadeCount> ShadowMatrices{};

	/** x: one divided by the width of the picture in pixels, y: one divided by its height. */
	glm::vec4 ScreenParameters = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
};

static_assert(
	sizeof(FViewConstants) == 16 + 4 * 32 + 32 * 32 + 9 * 16 + 64 + 64 + 16 + 16 + 16 + 4 * 16 + 16 + 16 * 64 + 16,
	"FViewConstants must be laid out as the shaders declare it");
