#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Math/Matrix.h"

#include <glm/vec4.hpp>

/**
 * One drawn mesh, as the shaders receive it. The renderer fills an array of these for every picture.
 * Must match FDrawData in Engine/Shaders/SceneData.glsl, member for member.
 */
struct FDrawData
{
	/** Takes a position in the mesh's own space to clip space. */
	FMatrix4f LocalToClip = FMatrix4f(1.0f);

	/** Takes a position in the mesh's own space to camera-relative space. */
	FMatrix4f LocalToRelative = FMatrix4f(1.0f);

	/** Takes a normal in the mesh's own space to camera-relative space. The result is not of unit length. */
	FMatrix4f NormalToRelative = FMatrix4f(1.0f);

	/** The base colour of the material, in linear space. */
	FLinearColor BaseColor;

	/** The light the surface gives off by itself. */
	FLinearColor EmissiveColor;

	/** x: metallic, y: roughness. */
	glm::vec4 MetallicRoughness = glm::vec4(0.0f);

	/**
	 * x: how strongly the normal texture bends the surface's normal. y: 1 if the material has a normal texture.
	 * z: how strongly the occlusion texture dims the light of the surroundings.
	 */
	glm::vec4 NormalScale = glm::vec4(1.0f, 0.0f, 1.0f, 0.0f);
};

static_assert(sizeof(FDrawData) == 256, "FDrawData must be laid out as the shaders declare it");
