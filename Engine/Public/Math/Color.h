#pragma once

#include "Core/CoreTypes.h"

/**
 * A colour with red, green, blue and alpha as bytes, as it is stored in an image file or shown on a screen: the
 * colour channels are sRGB-encoded, alpha is linear. Use FLinearColor for anything that is calculated with.
 */
struct FColor
{
	uint8 R = 0;
	uint8 G = 0;
	uint8 B = 0;
	uint8 A = 255;

	constexpr FColor() = default;

	constexpr FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255)
		: R(InR)
		, G(InG)
		, B(InB)
		, A(InA)
	{
	}

	bool operator==(const FColor& Other) const = default;
};
