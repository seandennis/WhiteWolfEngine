#pragma once

#include "CoreMinimal.h"

/** What the numbers in a texture's colour channels mean. */
enum class ETextureColorSpace : uint8
{
	/**
	 * The channels hold sRGB-encoded colours, as image files normally do. They are decoded to linear values when
	 * the texture is read. For textures that are colours: base colour, emissive colour.
	 */
	Srgb,

	/**
	 * The channels hold plain numbers that are used as they are. For textures that are data: normals, roughness,
	 * metallic.
	 */
	Linear,
};
