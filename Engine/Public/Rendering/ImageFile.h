#pragma once

#include "CoreMinimal.h"

#include "Rendering/Image.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

/**
 * Reads pictures from PNG and JPEG files and writes them as PNG files.
 *
 * Whatever a file holds (grey or colour, with or without transparency, 8 or 16 bits per channel) is read as the
 * four 8-bit channels of an FImage. The numbers in the file are taken as they are: an image file normally holds
 * sRGB-encoded colours, and so does the FImage read from it.
 */
class FImageFile
{
public:
	/** The largest width or height of a picture that is read or written, in pixels. */
	static constexpr int32 MaxDimension = 16384;

	/**
	 * Reads a PNG or JPEG file.
	 *
	 * @param OutImage Receives the picture. Unchanged on failure.
	 * @param OutError Receives the reason on failure: the file cannot be read, is not a PNG or JPEG image, is
	 * damaged, or is larger than MaxDimension.
	 * @return True if the picture was read.
	 */
	ENGINE_API static bool Load(const std::filesystem::path& Path, FImage& OutImage, std::string& OutError);

	/**
	 * Reads a PNG or JPEG file that is already in memory, for example one embedded in another file.
	 *
	 * @param FileData The bytes of the file.
	 * @param OutImage Receives the picture. Unchanged on failure.
	 * @param OutError Receives the reason on failure.
	 * @return True if the picture was read.
	 */
	ENGINE_API static bool LoadFromMemory(std::span<const std::byte> FileData, FImage& OutImage, std::string& OutError);

	/**
	 * Writes a picture as a PNG file, which keeps every pixel exactly. The directory is created if needed, and an
	 * existing file is replaced only once the new one is complete.
	 *
	 * @param OutError Receives the reason on failure: the image is empty or larger than MaxDimension, or the file
	 * cannot be written.
	 * @return True if the file was written.
	 */
	ENGINE_API static bool SavePng(const FImage& Image, const std::filesystem::path& Path, std::string& OutError);

	/**
	 * Encodes a picture as the bytes of a PNG file.
	 *
	 * @param OutFileData Receives the bytes. Unchanged on failure.
	 * @param OutError Receives the reason on failure: the image is empty or larger than MaxDimension.
	 * @return True if the picture was encoded.
	 */
	ENGINE_API static bool SavePngToMemory(
		const FImage& Image, std::vector<std::byte>& OutFileData, std::string& OutError);
};
