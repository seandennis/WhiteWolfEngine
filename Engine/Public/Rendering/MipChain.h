#pragma once

#include "CoreMinimal.h"

#include "Rendering/Image.h"
#include "Rendering/TextureColorSpace.h"

#include <vector>

/**
 * Makes the smaller versions of a picture that a texture needs to look right from a distance: each half the width
 * and height of the one before, down to a single pixel.
 */
class FMipChain
{
public:
	/**
	 * Returns the picture followed by its smaller versions. Each is half the size of the one before, rounded down
	 * but never below one pixel; the last is one pixel by one pixel. A pixel of a smaller version is the average of
	 * the pixels it covers.
	 *
	 * @param ColorSpace For sRGB, colours are averaged as the amounts of light they stand for, which keeps the
	 * brightness of the picture; alpha, and every channel of a linear picture, is averaged as it is.
	 * @return The versions, largest first. Empty if the image is empty.
	 */
	ENGINE_API static std::vector<FImage> Build(const FImage& Image, ETextureColorSpace ColorSpace);
};
