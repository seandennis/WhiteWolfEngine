#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

class WTexture;

/**
 * What a surface looks like: the properties the renderer shades a mesh with.
 *
 * Materials follow the metallic-roughness model of glTF 2.0, so a material imported from a glTF file has the same
 * numbers here as in the file. Each plain value can be varied over the surface by a texture, which multiplies it;
 * a texture that is null, or has no picture, leaves the plain value as it is.
 *
 * One material can be used by any number of meshes in any number of scenes; changing it changes all of them.
 */
WCLASS()
class ENGINE_API WMaterial : public WObject
{
	WW_DECLARE_CLASS(WMaterial, WObject)

public:
	/**
	 * The colour of the surface itself, in linear space: the colour a matte surface reflects, or the tint of a
	 * metal's reflections. Alpha is not used yet: surfaces are opaque.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	FLinearColor BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/** 0 for a material that is not a metal, such as plastic, wood or stone; 1 for a metal. Rarely in between. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	float Metallic = 0.0f;

	/** How rough the surface is: 0 is a mirror, 1 is completely matte. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	float Roughness = 0.5f;

	/**
	 * The light the surface gives off by itself, in linear space, whether or not any light falls on it. Black for
	 * a surface that does not glow; values above one glow brighter than white.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	FLinearColor EmissiveColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/** Multiplies BaseColor. An sRGB texture. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	TObjectPtr<WTexture> BaseColorTexture;

	/** Its green channel multiplies Roughness and its blue channel multiplies Metallic. A linear texture. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	TObjectPtr<WTexture> MetallicRoughnessTexture;

	/**
	 * Bends the direction the surface faces from pixel to pixel, which gives flat triangles the lighting of a
	 * detailed surface. A linear texture of directions relative to the surface, as glTF defines them: red along
	 * the first texture coordinate, green up in the picture, blue away from the surface.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	TObjectPtr<WTexture> NormalTexture;

	/** How strongly NormalTexture bends the surface: 1 is as authored, 0 is not at all. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	float NormalScale = 1.0f;

	/** Multiplies EmissiveColor. An sRGB texture. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	TObjectPtr<WTexture> EmissiveTexture;
};
