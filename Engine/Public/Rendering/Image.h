#pragma once

#include "CoreMinimal.h"

#include "Math/Color.h"

#include <vector>

/**
 * A picture in memory: a rectangle of FColor pixels, stored row by row from the top left corner. It is what the
 * renderer hands back when asked what it drew.
 */
class FImage
{
public:
	/** An empty image, zero pixels wide and high. */
	FImage() = default;

	/** An image of the given size with every pixel set to Fill. A negative size is a fatal error. */
	ENGINE_API FImage(int32 InWidth, int32 InHeight, FColor Fill = FColor());

	/**
	 * An image of the given size made of existing pixels, row by row from the top. It is a fatal error if the
	 * number of pixels is not InWidth times InHeight.
	 */
	ENGINE_API FImage(int32 InWidth, int32 InHeight, std::vector<FColor> InPixels);

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
	ENGINE_API const FColor& GetPixel(int32 X, int32 Y) const;

	/** Sets the pixel in column X of row Y. Out of range is a fatal error. */
	ENGINE_API void SetPixel(int32 X, int32 Y, FColor Color);

	/** Returns every pixel, row by row from the top; the pixel at (X, Y) is element Y * GetWidth() + X. */
	const std::vector<FColor>& GetPixels() const
	{
		return Pixels;
	}

	bool operator==(const FImage& Other) const = default;

private:
	int32 Width = 0;
	int32 Height = 0;
	std::vector<FColor> Pixels;
};
