#include "Rendering/ImageFile.h"

#include "Core/FileHelper.h"
#include "Math/Color.h"
#include "Rendering/Image.h"
#include "Rendering/TestImageFiles.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** A picture in which every pixel differs, with every alpha value from transparent to opaque. */
	FImage MakeTestImage(int32 Width, int32 Height)
	{
		FImage Image(Width, Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Index = Y * Width + X;
				Image.SetPixel(X,
					Y,
					FColor(static_cast<uint8>(Index * 7 % 256),
						static_cast<uint8>(Index * 13 % 256),
						static_cast<uint8>(255 - Index % 256),
						static_cast<uint8>(Index * 3 % 256)));
			}
		}
		return Image;
	}

	template <std::size_t Size>
	std::span<const std::byte> AsBytes(const uint8 (&Array)[Size])
	{
		return std::as_bytes(std::span<const uint8>(Array));
	}
}

TEST_SUITE("FImageFile")
{
	TEST_CASE("A picture saved as PNG is read back exactly, alpha included")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Pictures" / "Test.png";
		const FImage Image = MakeTestImage(37, 19);
		std::string Error;

		REQUIRE(FImageFile::SavePng(Image, Path, Error));

		FImage Loaded;
		INFO("Error: ", Error);
		REQUIRE(FImageFile::Load(Path, Loaded, Error));
		CHECK(Loaded == Image);
	}

	TEST_CASE("A one by one picture and a wide and a tall one are saved and read back")
	{
		for (const FImage& Image : {MakeTestImage(1, 1), MakeTestImage(300, 1), MakeTestImage(1, 300)})
		{
			std::vector<std::byte> FileData;
			std::string Error;
			REQUIRE(FImageFile::SavePngToMemory(Image, FileData, Error));

			FImage Loaded;
			REQUIRE(FImageFile::LoadFromMemory(FileData, Loaded, Error));
			CHECK(Loaded == Image);
		}
	}

	TEST_CASE("A saved file is a PNG file")
	{
		std::vector<std::byte> FileData;
		std::string Error;

		REQUIRE(FImageFile::SavePngToMemory(MakeTestImage(4, 4), FileData, Error));

		// Every PNG file starts with these eight bytes.
		const uint8 Signature[] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
		REQUIRE(FileData.size() > sizeof(Signature));
		for (std::size_t Index = 0; Index < sizeof(Signature); ++Index)
		{
			CHECK(FileData[Index] == static_cast<std::byte>(Signature[Index]));
		}
	}

	TEST_CASE("A PNG file written by another program is read, and a file without alpha is opaque")
	{
		FImage Image;
		std::string Error;

		INFO("Error: ", Error);
		REQUIRE(FImageFile::LoadFromMemory(AsBytes(TestRgbPngFile), Image, Error));

		REQUIRE(Image.GetWidth() == 4);
		REQUIRE(Image.GetHeight() == 2);
		// Row 0 is the top row of the picture.
		CHECK(Image.GetPixel(0, 0) == FColor(255, 0, 0, 255));
		CHECK(Image.GetPixel(1, 0) == FColor(0, 255, 0, 255));
		CHECK(Image.GetPixel(2, 0) == FColor(0, 0, 255, 255));
		CHECK(Image.GetPixel(3, 0) == FColor(255, 255, 255, 255));
		CHECK(Image.GetPixel(0, 1) == FColor(0, 0, 0, 255));
		CHECK(Image.GetPixel(1, 1) == FColor(255, 255, 0, 255));
		CHECK(Image.GetPixel(2, 1) == FColor(0, 255, 255, 255));
		CHECK(Image.GetPixel(3, 1) == FColor(255, 0, 255, 255));
	}

	TEST_CASE("A JPEG file is read")
	{
		FImage Image;
		std::string Error;

		INFO("Error: ", Error);
		REQUIRE(FImageFile::LoadFromMemory(AsBytes(TestSolidJpegFile), Image, Error));

		REQUIRE(Image.GetWidth() == 16);
		REQUIRE(Image.GetHeight() == 16);
		// JPEG does not keep colours exactly; a single colour comes back within a few steps.
		for (const FColor& Pixel : Image.GetPixels())
		{
			CHECK(std::abs(Pixel.R - 200) <= 3);
			CHECK(std::abs(Pixel.G - 100) <= 3);
			CHECK(std::abs(Pixel.B - 50) <= 3);
			CHECK(Pixel.A == 255);
		}
	}

	TEST_CASE("A file is read from disk like one in memory, and the path may contain non-English letters")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path =
			TempDirectory.GetPath() / std::filesystem::path(u8"Bilder") / u8"Stra\u00dfe.png";
		const std::string_view FileText(reinterpret_cast<const char*>(TestRgbPngFile), sizeof(TestRgbPngFile));
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile(FileText, Path, Error));

		FImage Image;
		REQUIRE(FImageFile::Load(Path, Image, Error));

		CHECK(Image.GetWidth() == 4);
		CHECK(Image.GetPixel(1, 1) == FColor(255, 255, 0, 255));
	}

	TEST_CASE("Saving replaces an existing file")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Test.png";
		std::string Error;
		REQUIRE(FImageFile::SavePng(MakeTestImage(8, 8), Path, Error));

		const FImage Second(3, 5, FColor(10, 20, 30, 40));
		REQUIRE(FImageFile::SavePng(Second, Path, Error));

		FImage Loaded;
		REQUIRE(FImageFile::Load(Path, Loaded, Error));
		CHECK(Loaded == Second);
	}

	TEST_CASE("A file that does not exist is an error that names it, and the image is left alone")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Missing.png";
		FImage Image(2, 2, FColor(1, 2, 3, 4));
		std::string Error;

		CHECK_FALSE(FImageFile::Load(Path, Image, Error));

		CHECK(Error.find("Missing.png") != std::string::npos);
		CHECK(Image == FImage(2, 2, FColor(1, 2, 3, 4)));
	}

	TEST_CASE("A file that is not an image is an error that names it")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Notes.png";
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile("This is not a picture.", Path, Error));
		FImage Image(2, 2, FColor(1, 2, 3, 4));

		CHECK_FALSE(FImageFile::Load(Path, Image, Error));

		CHECK(Error.find("Notes.png") != std::string::npos);
		CHECK(Error.find("not a PNG or JPEG image") != std::string::npos);
		CHECK(Image == FImage(2, 2, FColor(1, 2, 3, 4)));
	}

	TEST_CASE("Empty data and damaged files are errors")
	{
		FImage Image(2, 2, FColor(1, 2, 3, 4));
		std::string Error;

		CHECK_FALSE(FImageFile::LoadFromMemory({}, Image, Error));
		CHECK(Error == "the image file is empty");

		// A PNG file that ends in the middle of its pixel data.
		const std::span<const std::byte> Whole = AsBytes(TestRgbPngFile);
		CHECK_FALSE(FImageFile::LoadFromMemory(Whole.first(Whole.size() - 40), Image, Error));
		CHECK(Error.starts_with("the image is damaged"));

		// Only the eight bytes that say "this is a PNG file".
		CHECK_FALSE(FImageFile::LoadFromMemory(Whole.first(8), Image, Error));

		// A JPEG file cut off after its headers.
		const std::span<const std::byte> Jpeg = AsBytes(TestSolidJpegFile);
		CHECK_FALSE(FImageFile::LoadFromMemory(Jpeg.first(Jpeg.size() / 2), Image, Error));

		CHECK(Image == FImage(2, 2, FColor(1, 2, 3, 4)));
	}

	TEST_CASE("A file that claims a picture larger than the limit is refused before it is decoded")
	{
		// The header of a PNG file gives the size as two big-endian numbers, 16 bytes into the file. This one is
		// made to claim a width of 65536 pixels.
		std::vector<std::byte> FileData;
		for (const std::byte Byte : AsBytes(TestRgbPngFile))
		{
			FileData.push_back(Byte);
		}
		FileData[16] = std::byte{0x00};
		FileData[17] = std::byte{0x01};
		FileData[18] = std::byte{0x00};
		FileData[19] = std::byte{0x00};
		FImage Image;
		std::string Error;

		CHECK_FALSE(FImageFile::LoadFromMemory(FileData, Image, Error));

		CHECK(Error == "the image is 65536 by 2 pixels; each side must be between 1 and 16384");
	}

	TEST_CASE("An empty image cannot be saved, and no file is written")
	{
		const FScopedTempDirectory TempDirectory;
		const std::filesystem::path Path = TempDirectory.GetPath() / "Empty.png";
		std::string Error;

		CHECK_FALSE(FImageFile::SavePng(FImage(), Path, Error));

		CHECK(Error == "an image of 0 by 0 pixels cannot be saved; each side must be between 1 and 16384");
		CHECK_FALSE(std::filesystem::exists(Path));

		std::vector<std::byte> FileData{std::byte{1}};
		CHECK_FALSE(FImageFile::SavePngToMemory(FImage(), FileData, Error));
		CHECK(FileData.size() == 1);
	}
}
