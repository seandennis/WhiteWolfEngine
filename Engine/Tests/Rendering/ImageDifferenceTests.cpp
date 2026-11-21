#include "Rendering/ImageDifference.h"

#include "Math/Color.h"
#include "Rendering/Image.h"

#include <doctest/doctest.h>

#include <string>

TEST_SUITE("FImageDifference")
{
	TEST_CASE("Equal pictures do not differ")
	{
		const FImage Image(8, 4, FColor(10, 20, 30, 40));

		const FImageDifference Difference = FImageDifference::Measure(Image, Image, 0);

		CHECK(Difference.bSameSize);
		CHECK(Difference.PixelCount == 32);
		CHECK(Difference.DifferentPixelCount == 0);
		CHECK(Difference.LargestDifference == 0);
		CHECK(Difference.FirstDifferentX == -1);
		CHECK(Difference.FirstDifferentY == -1);
		CHECK(Difference.IsWithin(0.0));
		CHECK(
			Difference.Describe() == "no pixel differs by more than the tolerance; the largest difference is 0 of 255");
	}

	TEST_CASE("Pictures of different sizes cannot match")
	{
		const FImageDifference Difference = FImageDifference::Measure(FImage(8, 4), FImage(4, 8), 255);

		CHECK_FALSE(Difference.bSameSize);
		CHECK_FALSE(Difference.IsWithin(1.0));
		CHECK(Difference.Describe() == "the pictures have different sizes");
	}

	TEST_CASE("A pixel differs if any channel differs by more than the tolerance")
	{
		const FImage Reference(4, 4, FColor(100, 100, 100, 100));
		FImage Actual = Reference;
		Actual.SetPixel(0, 0, FColor(103, 100, 100, 100));
		Actual.SetPixel(1, 0, FColor(100, 97, 100, 100));
		Actual.SetPixel(2, 1, FColor(100, 100, 104, 100));
		Actual.SetPixel(3, 3, FColor(100, 100, 100, 60));

		const FImageDifference Exact = FImageDifference::Measure(Actual, Reference, 0);
		CHECK(Exact.DifferentPixelCount == 4);
		CHECK(Exact.LargestDifference == 40);
		CHECK(Exact.FirstDifferentX == 0);
		CHECK(Exact.FirstDifferentY == 0);

		// With a tolerance of three steps, the first two pixels are close enough.
		const FImageDifference Tolerant = FImageDifference::Measure(Actual, Reference, 3);
		CHECK(Tolerant.DifferentPixelCount == 2);
		CHECK(Tolerant.LargestDifference == 40);
		CHECK(Tolerant.FirstDifferentX == 2);
		CHECK(Tolerant.FirstDifferentY == 1);
		CHECK(Tolerant.Describe() ==
			"2 of 16 pixels (12.50%) differ by more than the tolerance, the first at (2, 1); the largest difference is "
			"40 of 255");
	}

	TEST_CASE("The comparison is the same whichever picture is called the reference")
	{
		const FImage Dark(2, 2, FColor(0, 0, 0, 255));
		const FImage Bright(2, 2, FColor(255, 10, 0, 255));

		CHECK(FImageDifference::Measure(Dark, Bright, 20) == FImageDifference::Measure(Bright, Dark, 20));
		CHECK(FImageDifference::Measure(Dark, Bright, 20).DifferentPixelCount == 4);
	}

	TEST_CASE("A comparison passes while no more than the allowed fraction of pixels differs")
	{
		const FImage Reference(10, 10, FColor(0, 0, 0, 255));
		FImage Actual = Reference;
		for (int32 X = 0; X < 5; ++X)
		{
			Actual.SetPixel(X, 0, FColor(255, 255, 255, 255));
		}

		const FImageDifference Difference = FImageDifference::Measure(Actual, Reference, 8);

		CHECK(Difference.DifferentPixelCount == 5);
		CHECK(Difference.IsWithin(0.05));
		CHECK(Difference.IsWithin(0.5));
		CHECK_FALSE(Difference.IsWithin(0.049));
		CHECK_FALSE(Difference.IsWithin(0.0));
	}

	TEST_CASE("Two empty pictures match")
	{
		const FImageDifference Difference = FImageDifference::Measure(FImage(), FImage(), 0);

		CHECK(Difference.bSameSize);
		CHECK(Difference.PixelCount == 0);
		CHECK(Difference.IsWithin(0.0));
	}
}
