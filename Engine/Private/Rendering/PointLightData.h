#pragma once

#include "CoreMinimal.h"

#include <glm/vec4.hpp>

/**
 * A point light, as the shaders receive it.
 * Must match FPointLightData in Engine/Shaders/SceneData.glsl, member for member.
 */
struct FPointLightData
{
	/** xyz: where the light is, relative to the camera. w: the distance at which it stops, or zero for no limit. */
	glm::vec4 LocationAndRange = glm::vec4(0.0f);

	/** rgb: colour times luminous intensity. */
	glm::vec4 Intensity = glm::vec4(0.0f);
};

static_assert(sizeof(FPointLightData) == 32, "FPointLightData must be laid out as the shaders declare it");
