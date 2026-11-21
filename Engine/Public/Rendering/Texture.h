#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Rendering/Image.h"
#include "Rendering/TextureColorSpace.h"
#include "Rendering/TextureWrap.h"

#include <string>

/**
 * A picture that materials put on surfaces: a colour pattern, or data such as surface normals or roughness.
 *
 * The texture holds its picture in main memory. The renderer copies it to the graphics card, with smaller versions
 * for viewing it from a distance, the first time it is used and again whenever the picture is replaced.
 */
WCLASS()
class ENGINE_API WTexture : public WObject
{
	WW_DECLARE_CLASS(WTexture, WObject)

public:
	/** The largest width or height a texture can have, in pixels. */
	static constexpr int32 MaxDimension = 16384;

	/**
	 * Replaces the picture of the texture.
	 *
	 * @param ColorSpace What the picture's colour channels mean: sRGB for colours, linear for data.
	 * @param OutError Receives the reason when the picture is refused: it is empty, or a side is longer than
	 * MaxDimension.
	 * @return True if the texture now has the given picture. Otherwise it is left as it was.
	 */
	bool SetImage(FImage InImage, ETextureColorSpace InColorSpace, std::string& OutError);

	/** Returns true once the texture has been given a picture. A texture without one counts as no texture. */
	bool HasImage() const;

	const FImage& GetImage() const;

	ETextureColorSpace GetColorSpace() const;

	/**
	 * Returns a number that identifies the picture the texture has now. It changes every time SetImage succeeds
	 * and is never the same for two textures, so the renderer can tell whether its copy on the graphics card is
	 * current. Zero means the texture has no picture.
	 */
	uint64 GetImageVersion() const;

	/** What the texture shows to the left and right of its picture, where the first texture coordinate leaves 0 to 1.
	 */
	ETextureWrap WrapU = ETextureWrap::Repeat;

	/** What the texture shows above and below its picture, where the second texture coordinate leaves 0 to 1. */
	ETextureWrap WrapV = ETextureWrap::Repeat;

private:
	FImage Image;
	ETextureColorSpace ColorSpace = ETextureColorSpace::Srgb;
	uint64 ImageVersion = 0;
};
