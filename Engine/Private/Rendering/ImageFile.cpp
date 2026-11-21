#include "Rendering/ImageFile.h"

#include "Core/FileHelper.h"
#include "Math/Color.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <cstring>
#include <format>
#include <limits>
#include <string_view>
#include <utility>

// Pixels are handed to and taken from the image library as four bytes each.
static_assert(sizeof(FColor) == 4, "FColor must be four bytes: red, green, blue, alpha");

namespace
{
	constexpr int ChannelsPerPixel = 4;

	bool IsSupportedSize(int Width, int Height)
	{
		return Width >= 1 && Width <= FImageFile::MaxDimension && Height >= 1 && Height <= FImageFile::MaxDimension;
	}

	/** Receives the bytes of a PNG file from the image library as it encodes one. */
	void AppendEncodedBytes(void* Context, void* Data, int Size)
	{
		std::vector<std::byte>& FileData = *static_cast<std::vector<std::byte>*>(Context);
		const std::byte* const Bytes = static_cast<const std::byte*>(Data);
		FileData.insert(FileData.end(), Bytes, Bytes + Size);
	}
}

bool FImageFile::Load(const std::filesystem::path& Path, FImage& OutImage, std::string& OutError)
{
	std::string FileText;
	if (!FFileHelper::LoadFileToString(Path, FileText, OutError))
	{
		return false;
	}

	const std::span<const std::byte> FileData = std::as_bytes(std::span<const char>(FileText));
	std::string DecodeError;
	if (!LoadFromMemory(FileData, OutImage, DecodeError))
	{
		OutError = std::format("'{}': {}", FFileHelper::PathToUtf8(Path), DecodeError);
		return false;
	}

	return true;
}

bool FImageFile::LoadFromMemory(std::span<const std::byte> FileData, FImage& OutImage, std::string& OutError)
{
	if (FileData.empty() || FileData.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		OutError = FileData.empty() ? "the image file is empty" : "the image file is too large";
		return false;
	}

	const stbi_uc* const Bytes = reinterpret_cast<const stbi_uc*>(FileData.data());
	const int ByteCount = static_cast<int>(FileData.size());

	// The size is read first, so that a file that claims an enormous picture is refused before memory is
	// allocated for it.
	int Width = 0;
	int Height = 0;
	int ChannelsInFile = 0;
	if (stbi_info_from_memory(Bytes, ByteCount, &Width, &Height, &ChannelsInFile) == 0)
	{
		OutError = "not a PNG or JPEG image";
		return false;
	}
	if (!IsSupportedSize(Width, Height))
	{
		OutError = std::format(
			"the image is {} by {} pixels; each side must be between 1 and {}", Width, Height, MaxDimension);
		return false;
	}

	stbi_uc* const Decoded =
		stbi_load_from_memory(Bytes, ByteCount, &Width, &Height, &ChannelsInFile, ChannelsPerPixel);
	if (Decoded == nullptr)
	{
		OutError = std::format("the image is damaged or of an unsupported kind ({})", stbi_failure_reason());
		return false;
	}

	std::vector<FColor> Pixels(static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height));
	std::memcpy(Pixels.data(), Decoded, Pixels.size() * sizeof(FColor));
	stbi_image_free(Decoded);

	OutImage = FImage(Width, Height, std::move(Pixels));
	return true;
}

bool FImageFile::SavePng(const FImage& Image, const std::filesystem::path& Path, std::string& OutError)
{
	std::vector<std::byte> FileData;
	if (!SavePngToMemory(Image, FileData, OutError))
	{
		return false;
	}

	const std::string_view FileText(reinterpret_cast<const char*>(FileData.data()), FileData.size());
	return FFileHelper::SaveStringToFile(FileText, Path, OutError);
}

bool FImageFile::SavePngToMemory(const FImage& Image, std::vector<std::byte>& OutFileData, std::string& OutError)
{
	if (!IsSupportedSize(Image.GetWidth(), Image.GetHeight()))
	{
		OutError = std::format("an image of {} by {} pixels cannot be saved; each side must be between 1 and {}",
			Image.GetWidth(),
			Image.GetHeight(),
			MaxDimension);
		return false;
	}

	std::vector<std::byte> FileData;
	const int BytesPerRow = Image.GetWidth() * ChannelsPerPixel;
	const int EncodeResult = stbi_write_png_to_func(&AppendEncodedBytes,
		&FileData,
		Image.GetWidth(),
		Image.GetHeight(),
		ChannelsPerPixel,
		Image.GetPixels().data(),
		BytesPerRow);
	if (EncodeResult == 0 || FileData.empty())
	{
		OutError = "the image could not be encoded as PNG";
		return false;
	}

	OutFileData = std::move(FileData);
	return true;
}
