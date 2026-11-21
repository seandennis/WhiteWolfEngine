#pragma once

#include "CoreMinimal.h"

#include "Math/Matrix.h"
#include "Rendering/DirectionalLightData.h"
#include "Rendering/PointLightData.h"
#include "Rendering/SceneRenderer.h"

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
};

static_assert(sizeof(FViewConstants) == 16 + 4 * 32 + 32 * 32 + 9 * 16 + 64 + 64 + 16,
	"FViewConstants must be laid out as the shaders declare it");
