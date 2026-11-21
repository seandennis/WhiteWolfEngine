#pragma once

#include "CoreMinimal.h"

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
};

static_assert(
	sizeof(FViewConstants) == 16 + 4 * 32 + 32 * 32, "FViewConstants must be laid out as the shaders declare it");
