#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"

#include <vector>

/**
 * A picture of light: a rectangle of FLinearColor pixels, stored row by row from the top left corner. Unlike an
 * FImage, whose pixels are what a display shows, its pixels are amounts of light in linear space and may be far
 * brighter than white. A photograph of a whole surrounding, which lights a scene, is such a picture.
 */
class FHdrImage
{
public:
	/** An empty image, zero pixels wide and high. */
	FHdrImage() = default;

	/** An image of the given size with every pixel set to Fill. A negative size is a fatal error. */
	ENGINE_API FHdrImage(int32 InWidth, int32 InHeight, const FLinearColor& Fill = FLinearColor());

	/**
	 * An image of the given size made of existing pixels, row by row from the top. It is a fatal error if the
	 * number of pixels is not InWidth times InHeight.
	 */
	ENGINE_API FHdrImage(int32 InWidth, int32 InHeight, std::vector<FLinearColor> InPixels);

	int32 GetWidth() const
	{
		return Width;
	}

	int32 GetHeight() const
	{
		return Height;
	}

	/** Returns true if the image has no pixels. */
	bool IsEmpty() const
	{
		return Pixels.empty();
	}

	/** Returns the pixel in column X of row Y, counted from the top left. Out of range is a fatal error. */
	ENGINE_API const FLinearColor& GetPixel(int32 X, int32 Y) const;

	/** Sets the pixel in column X of row Y. Out of range is a fatal error. */
	ENGINE_API void SetPixel(int32 X, int32 Y, const FLinearColor& Color);

	/** Returns every pixel, row by row from the top; the pixel at (X, Y) is element Y * GetWidth() + X. */
	const std::vector<FLinearColor>& GetPixels() const
	{
		return Pixels;
	}

	bool operator==(const FHdrImage& Other) const = default;

private:
	int32 Width = 0;
	int32 Height = 0;
	std::vector<FLinearColor> Pixels;
};
