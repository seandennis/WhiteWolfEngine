#include "Rendering/Image.h"

#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <vector>

TEST_SUITE("FImage")
{
	TEST_CASE("A default-constructed image is empty")
	{
		const FImage Image;

		CHECK(Image.IsEmpty());
		CHECK(Image.GetWidth() == 0);
		CHECK(Image.GetHeight() == 0);
		CHECK(Image.GetPixels().empty());
	}

	TEST_CASE("An image of a given size is filled with one colour")
	{
		const FImage Image(3, 2, FColor(10, 20, 30, 40));

		CHECK_FALSE(Image.IsEmpty());
		CHECK(Image.GetWidth() == 3);
		CHECK(Image.GetHeight() == 2);
		CHECK(Image.GetPixels().size() == 6);
		CHECK(Image.GetPixel(0, 0) == FColor(10, 20, 30, 40));
		CHECK(Image.GetPixel(2, 1) == FColor(10, 20, 30, 40));
	}

	TEST_CASE("The default fill is opaque black")
	{
		const FImage Image(1, 1);

		CHECK(Image.GetPixel(0, 0) == FColor(0, 0, 0, 255));
	}

	TEST_CASE("An image with a side of zero has no pixels")
	{
		CHECK(FImage(0, 5).IsEmpty());
		CHECK(FImage(5, 0).IsEmpty());
	}

	TEST_CASE("SetPixel changes one pixel, addressed by column and row from the top left")
	{
		FImage Image(3, 2);

		Image.SetPixel(2, 0, FColor(255, 0, 0));
		Image.SetPixel(0, 1, FColor(0, 255, 0));

		CHECK(Image.GetPixel(2, 0) == FColor(255, 0, 0));
		CHECK(Image.GetPixel(0, 1) == FColor(0, 255, 0));
		CHECK(Image.GetPixel(1, 0) == FColor());
		// Rows are stored one after the other.
		CHECK(Image.GetPixels()[2] == FColor(255, 0, 0));
		CHECK(Image.GetPixels()[3] == FColor(0, 255, 0));
	}

	TEST_CASE("An image can be made from existing pixels")
	{
		const std::vector<FColor> Pixels{FColor(1, 1, 1), FColor(2, 2, 2), FColor(3, 3, 3), FColor(4, 4, 4)};

		const FImage Image(2, 2, Pixels);

		CHECK(Image.GetPixel(1, 0) == FColor(2, 2, 2));
		CHECK(Image.GetPixel(0, 1) == FColor(3, 3, 3));
	}

	TEST_CASE("Images compare by size and pixels")
	{
		FImage First(2, 2, FColor(9, 9, 9));
		const FImage Same(2, 2, FColor(9, 9, 9));
		const FImage OtherShape(4, 1, FColor(9, 9, 9));

		CHECK(First == Same);
		CHECK_FALSE(First == OtherShape);

		First.SetPixel(1, 1, FColor(0, 0, 0));
		CHECK_FALSE(First == Same);
	}

	TEST_CASE("A negative size, a wrong number of pixels and a pixel out of range are fatal errors")
	{
		const FScopedLogCapture Capture;
		FImage Image(2, 2);
		const std::vector<FColor> ThreePixels(3);

		CHECK_THROWS_AS(FImage(-1, 2), FFatalErrorException);
		CHECK_THROWS_AS(FImage(2, -1), FFatalErrorException);
		CHECK_THROWS_AS(FImage(2, 2, ThreePixels), FFatalErrorException);
		CHECK_THROWS_AS(Image.GetPixel(2, 0), FFatalErrorException);
		CHECK_THROWS_AS(Image.GetPixel(0, 2), FFatalErrorException);
		CHECK_THROWS_AS(Image.GetPixel(-1, 0), FFatalErrorException);
		CHECK_THROWS_AS(Image.SetPixel(0, -1, FColor()), FFatalErrorException);
	}
}
