#include "Rendering/HdrImage.h"

#include "Math/LinearColor.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <vector>

TEST_SUITE("FHdrImage")
{
	TEST_CASE("A new image is empty")
	{
		const FHdrImage Image;

		CHECK(Image.IsEmpty());
		CHECK(Image.GetWidth() == 0);
		CHECK(Image.GetHeight() == 0);
		CHECK(Image.GetPixels().empty());
	}

	TEST_CASE("An image of a size is filled with one amount of light, which may be far brighter than white")
	{
		const FHdrImage Image(3, 2, FLinearColor(50000.0f, 2.0f, 0.25f));

		CHECK_FALSE(Image.IsEmpty());
		CHECK(Image.GetWidth() == 3);
		CHECK(Image.GetHeight() == 2);
		REQUIRE(Image.GetPixels().size() == 6);
		CHECK(Image.GetPixel(2, 1) == FLinearColor(50000.0f, 2.0f, 0.25f));
	}

	TEST_CASE("Pixels are stored row by row from the top left")
	{
		FHdrImage Image(2, 2);
		Image.SetPixel(1, 0, FLinearColor(1.0f, 0.0f, 0.0f));
		Image.SetPixel(0, 1, FLinearColor(0.0f, 1.0f, 0.0f));

		CHECK(Image.GetPixels()[1] == FLinearColor(1.0f, 0.0f, 0.0f));
		CHECK(Image.GetPixels()[2] == FLinearColor(0.0f, 1.0f, 0.0f));
		CHECK(Image ==
			FHdrImage(2,
				2,
				std::vector<FLinearColor>{
					FLinearColor(), FLinearColor(1.0f, 0.0f, 0.0f), FLinearColor(0.0f, 1.0f, 0.0f), FLinearColor()}));
	}

	TEST_CASE("Impossible sizes and pixels outside the image are programming errors")
	{
		const FScopedLogCapture Capture;
		FHdrImage Image(2, 2);

		CHECK_THROWS_AS(FHdrImage(-1, 2), FFatalErrorException);
		CHECK_THROWS_AS(FHdrImage(2, 2, std::vector<FLinearColor>(3)), FFatalErrorException);
		CHECK_THROWS_AS(Image.GetPixel(2, 0), FFatalErrorException);
		CHECK_THROWS_AS(Image.GetPixel(0, -1), FFatalErrorException);
		CHECK_THROWS_AS(Image.SetPixel(0, 2, FLinearColor()), FFatalErrorException);
	}
}
