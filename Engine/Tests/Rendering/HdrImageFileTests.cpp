#include "Core/FileHelper.h"
#include "Math/LinearColor.h"
#include "Rendering/HdrImage.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** The format keeps a pixel to about one part in two hundred of its brightest channel. */
	bool IsNearlyEqual(const FLinearColor& Actual, const FLinearColor& Expected)
	{
		const float Brightest = std::max({Expected.R, Expected.G, Expected.B});
		const float Tolerance = Brightest / 128.0f + 1.0e-6f;
		return std::abs(Actual.R - Expected.R) <= Tolerance && std::abs(Actual.G - Expected.G) <= Tolerance &&
			std::abs(Actual.B - Expected.B) <= Tolerance && Actual.A == 1.0f;
	}

	/** A picture with every kind of brightness: black, dim, ordinary, and a sun thousands of times white. */
	FHdrImage MakeTestImage(int32 Width, int32 Height)
	{
		FHdrImage Image(Width, Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float Across = static_cast<float>(X) / static_cast<float>(Width);
				const float Down = static_cast<float>(Y) / static_cast<float>(Height);
				Image.SetPixel(X, Y, FLinearColor(Across * 4.0f, Down * 0.01f, 0.5f));
			}
		}
		Image.SetPixel(0, 0, FLinearColor(0.0f, 0.0f, 0.0f));
		Image.SetPixel(Width - 1, 0, FLinearColor(60000.0f, 45000.0f, 30000.0f));
		return Image;
	}
}

TEST_SUITE("FImageFile HDR")
{
	TEST_CASE("A picture of light saved as a Radiance HDR file is read back, closely")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Skies" / "Noon.hdr";
		const FHdrImage Image = MakeTestImage(40, 20);
		std::string Error;

		REQUIRE(FImageFile::SaveHdr(Image, Path, Error));

		FHdrImage Loaded;
		INFO("Error: ", Error);
		REQUIRE(FImageFile::LoadHdr(Path, Loaded, Error));
		REQUIRE(Loaded.GetWidth() == 40);
		REQUIRE(Loaded.GetHeight() == 20);
		int32 WrongPixelCount = 0;
		for (std::size_t Index = 0; Index < Image.GetPixels().size(); ++Index)
		{
			WrongPixelCount += IsNearlyEqual(Loaded.GetPixels()[Index], Image.GetPixels()[Index]) ? 0 : 1;
		}
		CHECK(WrongPixelCount == 0);

		// Black stays black, and light thousands of times brighter than white is not cut off.
		CHECK(Loaded.GetPixel(0, 0) == FLinearColor(0.0f, 0.0f, 0.0f));
		CHECK(Loaded.GetPixel(39, 0).R > 59000.0f);
	}

	TEST_CASE("Each pixel keeps its own range, so a dim pixel beside a bright one stays precise")
	{
		// The format stores a pixel as three numbers of 256 steps and a common scale. A pixel a million times
		// dimmer than its neighbour is therefore kept as precisely as the bright one.
		FHdrImage Image(2, 1);
		Image.SetPixel(0, 0, FLinearColor(0.001f, 0.0005f, 0.00025f));
		Image.SetPixel(1, 0, FLinearColor(1024.0f, 512.0f, 16.0f));
		std::vector<std::byte> FileData;
		std::string Error;

		REQUIRE(FImageFile::SaveHdrToMemory(Image, FileData, Error));
		FHdrImage Loaded;
		REQUIRE(FImageFile::LoadHdrFromMemory(FileData, Loaded, Error));

		REQUIRE(Loaded.GetWidth() == 2);
		CHECK(IsNearlyEqual(Loaded.GetPixel(0, 0), Image.GetPixel(0, 0)));
		CHECK(IsNearlyEqual(Loaded.GetPixel(1, 0), Image.GetPixel(1, 0)));
	}

	TEST_CASE("Alpha is not saved, and values a file cannot hold are written as zero")
	{
		FHdrImage Image(1, 1);
		Image.SetPixel(0, 0, FLinearColor(-5.0f, std::numeric_limits<float>::quiet_NaN(), 2.0f, 0.25f));
		std::vector<std::byte> FileData;
		std::string Error;

		REQUIRE(FImageFile::SaveHdrToMemory(Image, FileData, Error));
		FHdrImage Loaded;
		REQUIRE(FImageFile::LoadHdrFromMemory(FileData, Loaded, Error));

		CHECK(IsNearlyEqual(Loaded.GetPixel(0, 0), FLinearColor(0.0f, 0.0f, 2.0f, 1.0f)));
	}

	TEST_CASE("An ordinary image is not a picture of light")
	{
		std::vector<std::byte> PngData;
		std::string Error;
		REQUIRE(FImageFile::SavePngToMemory(FImage(4, 4, FColor(255, 0, 0)), PngData, Error));
		FHdrImage Image(1, 1, FLinearColor(1.0f, 2.0f, 3.0f));

		CHECK_FALSE(FImageFile::LoadHdrFromMemory(PngData, Image, Error));

		CHECK(Error == "not a Radiance HDR image");
		CHECK(Image == FHdrImage(1, 1, FLinearColor(1.0f, 2.0f, 3.0f)));
	}

	TEST_CASE("Missing, empty and damaged files are errors, and the image is left alone")
	{
		const FScopedTempDirectory TempDirectory;
		FHdrImage Image(1, 1, FLinearColor(1.0f, 2.0f, 3.0f));
		std::string Error;

		CHECK_FALSE(FImageFile::LoadHdr(TempDirectory.GetPath() / "Missing.hdr", Image, Error));
		CHECK(Error.find("Missing.hdr") != std::string::npos);

		CHECK_FALSE(FImageFile::LoadHdrFromMemory({}, Image, Error));
		CHECK(Error == "the image file is empty");

		const std::filesystem::path NotesPath = TempDirectory.GetPath() / "Notes.hdr";
		REQUIRE(FFileHelper::SaveStringToFile("This is not a picture.", NotesPath, Error));
		CHECK_FALSE(FImageFile::LoadHdr(NotesPath, Image, Error));
		CHECK(Error.find("Notes.hdr") != std::string::npos);
		CHECK(Error.find("not a Radiance HDR image") != std::string::npos);

		// A file that ends in the middle of its pixels.
		std::vector<std::byte> FileData;
		REQUIRE(FImageFile::SaveHdrToMemory(MakeTestImage(64, 32), FileData, Error));
		const std::span<const std::byte> Whole(FileData);
		CHECK_FALSE(FImageFile::LoadHdrFromMemory(Whole.first(Whole.size() / 2), Image, Error));
		CHECK(Error.starts_with("the image is damaged"));

		CHECK(Image == FHdrImage(1, 1, FLinearColor(1.0f, 2.0f, 3.0f)));
	}

	TEST_CASE("An empty picture of light cannot be saved, and no file is written")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Empty.hdr";
		std::string Error;

		CHECK_FALSE(FImageFile::SaveHdr(FHdrImage(), Path, Error));

		CHECK(Error == "an image of 0 by 0 pixels cannot be saved; each side must be between 1 and 16384");
		CHECK_FALSE(std::filesystem::exists(Path));
	}
}
