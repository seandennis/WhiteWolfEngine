#pragma once

#include "CoreMinimal.h"

#include "Rendering/Image.h"

#include <string>

/**
 * How much two pictures differ. This is what a screenshot is compared to its reference image with: two graphics
 * cards never draw exactly the same picture, so a comparison counts the pixels that differ noticeably instead of
 * demanding that all are equal.
 */
struct FImageDifference
{
	/** False if the pictures have different sizes; then nothing else was compared. */
	bool bSameSize = false;

	/** How many pixels each picture has, if they have the same size. */
	int32 PixelCount = 0;

	/** How many pixels have a channel that differs by more than the tolerance. */
	int32 DifferentPixelCount = 0;

	/** The largest difference of any channel of any pixel, in steps of 255. */
	int32 LargestDifference = 0;

	/** Where the first pixel that differs by more than the tolerance is, row by row from the top; -1 if none does. */
	int32 FirstDifferentX = -1;
	int32 FirstDifferentY = -1;

	/**
	 * Compares two pictures pixel by pixel.
	 *
	 * @param ChannelTolerance By how many steps of 255 a channel may differ before its pixel counts as different.
	 */
	ENGINE_API static FImageDifference Measure(const FImage& Actual, const FImage& Reference, int32 ChannelTolerance);

	/**
	 * Returns true if the pictures have the same size and at most the given fraction of their pixels differ by
	 * more than the tolerance they were measured with. A fraction of 0.01 allows one pixel in a hundred.
	 */
	ENGINE_API bool IsWithin(double MaxDifferentPixelFraction) const;

	/** Returns the difference in words, for the message of a failed comparison. */
	ENGINE_API std::string Describe() const;

	bool operator==(const FImageDifference& Other) const = default;
};
