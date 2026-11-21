#include "Rendering/HdrImage.h"

#include <cstddef>
#include <utility>

FHdrImage::FHdrImage(int32 InWidth, int32 InHeight, const FLinearColor& Fill)
	: Width(InWidth)
	, Height(InHeight)
{
	WW_CHECKF(InWidth >= 0 && InHeight >= 0, "An image cannot be {} by {} pixels", InWidth, InHeight);
	Pixels.assign(static_cast<std::size_t>(InWidth) * static_cast<std::size_t>(InHeight), Fill);
}

FHdrImage::FHdrImage(int32 InWidth, int32 InHeight, std::vector<FLinearColor> InPixels)
	: Width(InWidth)
	, Height(InHeight)
	, Pixels(std::move(InPixels))
{
	WW_CHECKF(InWidth >= 0 && InHeight >= 0, "An image cannot be {} by {} pixels", InWidth, InHeight);
	WW_CHECKF(Pixels.size() == static_cast<std::size_t>(InWidth) * static_cast<std::size_t>(InHeight),
		"An image of {} by {} pixels was given {} pixels",
		InWidth,
		InHeight,
		Pixels.size());
}

const FLinearColor& FHdrImage::GetPixel(int32 X, int32 Y) const
{
	WW_CHECKF(X >= 0 && X < Width && Y >= 0 && Y < Height,
		"Pixel ({}, {}) is outside an image of {} by {} pixels",
		X,
		Y,
		Width,
		Height);
	return Pixels[static_cast<std::size_t>(Y) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(X)];
}

void FHdrImage::SetPixel(int32 X, int32 Y, const FLinearColor& Color)
{
	WW_CHECKF(X >= 0 && X < Width && Y >= 0 && Y < Height,
		"Pixel ({}, {}) is outside an image of {} by {} pixels",
		X,
		Y,
		Width,
		Height);
	Pixels[static_cast<std::size_t>(Y) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(X)] = Color;
}
