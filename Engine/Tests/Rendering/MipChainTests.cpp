#include "Rendering/MipChain.h"

#include "Math/Color.h"
#include "Rendering/Image.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

namespace
{
	/** The sizes of a chain, largest first. */
	std::vector<std::pair<int32, int32>> GetSizes(const std::vector<FImage>& Chain)
	{
		std::vector<std::pair<int32, int32>> Sizes;
		for (const FImage& Image : Chain)
		{
			Sizes.emplace_back(Image.GetWidth(), Image.GetHeight());
		}
		return Sizes;
	}

	bool IsWithinOneStep(FColor Actual, FColor Expected)
	{
		return std::abs(Actual.R - Expected.R) <= 1 && std::abs(Actual.G - Expected.G) <= 1 &&
			std::abs(Actual.B - Expected.B) <= 1 && std::abs(Actual.A - Expected.A) <= 1;
	}
}

TEST_SUITE("FMipChain")
{
	TEST_CASE("An empty image has no versions")
	{
		CHECK(FMipChain::Build(FImage(), ETextureColorSpace::Srgb).empty());
	}

	TEST_CASE("A single pixel is its own and only version")
	{
		const FImage Image(1, 1, FColor(10, 20, 30, 40));

		const std::vector<FImage> Chain = FMipChain::Build(Image, ETextureColorSpace::Linear);

		REQUIRE(Chain.size() == 1);
		CHECK(Chain[0] == Image);
	}

	TEST_CASE("Each version is half the size of the one before, down to one pixel")
	{
		using FSizes = std::vector<std::pair<int32, int32>>;

		CHECK(GetSizes(FMipChain::Build(FImage(8, 8), ETextureColorSpace::Srgb)) ==
			FSizes{{8, 8}, {4, 4}, {2, 2}, {1, 1}});
		// A side that reaches one pixel stays there while the other goes on halving.
		CHECK(GetSizes(FMipChain::Build(FImage(8, 2), ETextureColorSpace::Srgb)) ==
			FSizes{{8, 2}, {4, 1}, {2, 1}, {1, 1}});
		CHECK(GetSizes(FMipChain::Build(FImage(1, 4), ETextureColorSpace::Srgb)) == FSizes{{1, 4}, {1, 2}, {1, 1}});
		// Odd sizes round down.
		CHECK(GetSizes(FMipChain::Build(FImage(7, 5), ETextureColorSpace::Srgb)) == FSizes{{7, 5}, {3, 2}, {1, 1}});
	}

	TEST_CASE("The first version is the picture itself")
	{
		FImage Image(4, 4, FColor(1, 2, 3, 4));
		Image.SetPixel(2, 1, FColor(200, 100, 50, 25));

		const std::vector<FImage> Chain = FMipChain::Build(Image, ETextureColorSpace::Srgb);

		REQUIRE_FALSE(Chain.empty());
		CHECK(Chain[0] == Image);
	}

	TEST_CASE("A picture of one colour keeps that colour in every version")
	{
		for (const ETextureColorSpace ColorSpace : {ETextureColorSpace::Srgb, ETextureColorSpace::Linear})
		{
			const FColor Color(200, 100, 50, 150);

			const std::vector<FImage> Chain = FMipChain::Build(FImage(13, 6, Color), ColorSpace);

			for (const FImage& Image : Chain)
			{
				for (const FColor& Pixel : Image.GetPixels())
				{
					CHECK(Pixel == Color);
				}
			}
		}
	}

	TEST_CASE("A pixel of a linear picture's smaller version is the plain average of the four it covers")
	{
		FImage Image(4, 2);
		Image.SetPixel(0, 0, FColor(0, 0, 0, 0));
		Image.SetPixel(1, 0, FColor(100, 40, 255, 255));
		Image.SetPixel(0, 1, FColor(200, 80, 255, 255));
		Image.SetPixel(1, 1, FColor(100, 40, 255, 0));
		for (int32 Y = 0; Y < 2; ++Y)
		{
			Image.SetPixel(2, Y, FColor(8, 16, 32, 64));
			Image.SetPixel(3, Y, FColor(8, 16, 32, 64));
		}

		const std::vector<FImage> Chain = FMipChain::Build(Image, ETextureColorSpace::Linear);

		REQUIRE(Chain.size() == 3);
		CHECK(IsWithinOneStep(Chain[1].GetPixel(0, 0), FColor(100, 40, 191, 128)));
		CHECK(Chain[1].GetPixel(1, 0) == FColor(8, 16, 32, 64));
	}

	TEST_CASE("Colours of an sRGB picture are averaged as light, alpha as it is")
	{
		// Half black and half white pixels reflect half the light, which is 188 as an sRGB value, not 128.
		FImage Image(2, 2);
		Image.SetPixel(0, 0, FColor(0, 0, 0, 0));
		Image.SetPixel(1, 0, FColor(255, 255, 255, 255));
		Image.SetPixel(0, 1, FColor(255, 255, 255, 255));
		Image.SetPixel(1, 1, FColor(0, 0, 0, 0));

		const std::vector<FImage> Srgb = FMipChain::Build(Image, ETextureColorSpace::Srgb);
		const std::vector<FImage> Linear = FMipChain::Build(Image, ETextureColorSpace::Linear);

		REQUIRE(Srgb.size() == 2);
		REQUIRE(Linear.size() == 2);
		CHECK(Srgb[1].GetPixel(0, 0) == FColor(188, 188, 188, 128));
		CHECK(Linear[1].GetPixel(0, 0) == FColor(128, 128, 128, 128));
	}

	TEST_CASE("The last row and column of a picture of odd size are not left out")
	{
		// Three pixels wide: the single pixel of the half covers all three, including the bright last one.
		FImage Image(3, 1, FColor(0, 0, 0, 255));
		Image.SetPixel(2, 0, FColor(255, 255, 255, 255));

		const std::vector<FImage> Chain = FMipChain::Build(Image, ETextureColorSpace::Linear);

		REQUIRE(Chain.size() == 2);
		CHECK(Chain[1].GetPixel(0, 0) == FColor(85, 85, 85, 255));
	}
}
