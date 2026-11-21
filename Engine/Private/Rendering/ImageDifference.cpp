#include "Rendering/ImageDifference.h"

#include "Math/Color.h"

#include <algorithm>
#include <cstdlib>
#include <format>

FImageDifference FImageDifference::Measure(const FImage& Actual, const FImage& Reference, int32 ChannelTolerance)
{
	FImageDifference Difference;
	Difference.bSameSize = Actual.GetWidth() == Reference.GetWidth() && Actual.GetHeight() == Reference.GetHeight();
	if (!Difference.bSameSize)
	{
		return Difference;
	}

	Difference.PixelCount = Actual.GetWidth() * Actual.GetHeight();
	for (int32 Y = 0; Y < Actual.GetHeight(); ++Y)
	{
		for (int32 X = 0; X < Actual.GetWidth(); ++X)
		{
			const FColor& Ours = Actual.GetPixel(X, Y);
			const FColor& Theirs = Reference.GetPixel(X, Y);
			const int32 PixelDifference = std::max({std::abs(Ours.R - Theirs.R),
				std::abs(Ours.G - Theirs.G),
				std::abs(Ours.B - Theirs.B),
				std::abs(Ours.A - Theirs.A)});

			Difference.LargestDifference = std::max(Difference.LargestDifference, PixelDifference);
			if (PixelDifference > ChannelTolerance)
			{
				if (Difference.DifferentPixelCount == 0)
				{
					Difference.FirstDifferentX = X;
					Difference.FirstDifferentY = Y;
				}
				++Difference.DifferentPixelCount;
			}
		}
	}

	return Difference;
}

bool FImageDifference::IsWithin(double MaxDifferentPixelFraction) const
{
	if (!bSameSize)
	{
		return false;
	}
	if (PixelCount == 0)
	{
		return true;
	}

	return static_cast<double>(DifferentPixelCount) <= MaxDifferentPixelFraction * static_cast<double>(PixelCount);
}

std::string FImageDifference::Describe() const
{
	if (!bSameSize)
	{
		return "the pictures have different sizes";
	}
	if (DifferentPixelCount == 0)
	{
		return std::format(
			"no pixel differs by more than the tolerance; the largest difference is {} of 255", LargestDifference);
	}

	const double Percentage = 100.0 * static_cast<double>(DifferentPixelCount) / static_cast<double>(PixelCount);
	return std::format(
		"{} of {} pixels ({:.2f}%) differ by more than the tolerance, the first at ({}, {}); the largest "
		"difference is {} of 255",
		DifferentPixelCount,
		PixelCount,
		Percentage,
		FirstDifferentX,
		FirstDifferentY,
		LargestDifference);
}
