#pragma once

#include "CoreMinimal.h"

#include <glm/vec4.hpp>

/**
 * A directional light, as the shaders receive it.
 * Must match FDirectionalLightData in Engine/Shaders/SceneData.glsl, member for member.
 */
struct FDirectionalLightData
{
	/** xyz: the unit vector from a surface towards the light. */
	glm::vec4 DirectionToLight = glm::vec4(0.0f);

	/** rgb: the light arriving at a surface that faces the light, colour times illuminance. */
	glm::vec4 Illuminance = glm::vec4(0.0f);
};

static_assert(sizeof(FDirectionalLightData) == 32, "FDirectionalLightData must be laid out as the shaders declare it");
