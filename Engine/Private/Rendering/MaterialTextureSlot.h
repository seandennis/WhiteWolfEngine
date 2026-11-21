#pragma once

#include "CoreMinimal.h"

#include <cstddef>

/**
 * The textures a material can have, in the order the shaders declare them in
 * Engine/Shaders/MaterialTextures.glsl.
 */
enum class EMaterialTextureSlot : uint8
{
	BaseColor,
	MetallicRoughness,
	Normal,
	Emissive,
};

/** The number of values of EMaterialTextureSlot. */
inline constexpr std::size_t MaterialTextureSlotCount = 4;
