#pragma once

#include "Core/CoreTypes.h"

/**
 * A colour with red, green, blue and alpha as floating-point numbers in linear space, which is the space lighting
 * is calculated in. Values above one are valid and mean brighter than white. This is the colour type of materials,
 * lights and everything else the renderer computes with.
 */
struct FLinearColor
{
	float R = 0.0f;
	float G = 0.0f;
	float B = 0.0f;
	float A = 1.0f;

	constexpr FLinearColor() = default;

	constexpr FLinearColor(float InR, float InG, float InB, float InA = 1.0f)
		: R(InR)
		, G(InG)
		, B(InB)
		, A(InA)
	{
	}

	bool operator==(const FLinearColor& Other) const = default;
};
