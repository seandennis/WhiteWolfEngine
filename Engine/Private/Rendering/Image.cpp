#include "Rendering/Image.h"

#include <cstddef>
#include <utility>

FImage::FImage(int32 InWidth, int32 InHeight, FColor Fill)
	: Width(InWidth)
	, Height(InHeight)
{
	WW_CHECKF(InWidth >= 0 && InHeight >= 0, "An image cannot be {} by {} pixels", InWidth, InHeight);
	Pixels.assign(static_cast<std::size_t>(InWidth) * static_cast<std::size_t>(InHeight), Fill);
}

FImage::FImage(int32 InWidth, int32 InHeight, std::vector<FColor> InPixels)
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

const FColor& FImage::GetPixel(int32 X, int32 Y) const
{
	WW_CHECKF(X >= 0 && X < Width && Y >= 0 && Y < Height,
		"Pixel ({}, {}) is outside an image of {} by {} pixels",
		X,
		Y,
		Width,
		Height);
	return Pixels[static_cast<std::size_t>(Y) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(X)];
}

void FImage::SetPixel(int32 X, int32 Y, FColor Color)
{
	WW_CHECKF(X >= 0 && X < Width && Y >= 0 && Y < Height,
		"Pixel ({}, {}) is outside an image of {} by {} pixels",
		X,
		Y,
		Width,
		Height);
	Pixels[static_cast<std::size_t>(Y) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(X)] = Color;
}
