#pragma once

#include "Core/CoreTypes.h"

/**
 * The sRGB encoding of colours: how pictures, displays and colour pickers write an amount of light as a number.
 *
 * The encoding spends its numbers where the eye tells shades apart: the encoded value 0.5 stands for about a fifth
 * of the light of 1, not for half. Lighting is calculated with amounts of light (FLinearColor); colours are stored
 * in pictures, shown and picked encoded (FColor). Alpha is never encoded.
 */
class FSrgb
{
public:
	/**
	 * Returns the encoded value of an amount of light: 0 for 0 and 1 for 1. An amount above 1, brighter than white,
	 * goes on along the same curve, and a negative amount is encoded as its positive is, with its sign kept.
	 */
	ENGINE_API static float Encode(float Linear);

	/** Returns the amount of light that an encoded value stands for: what Encode gives that value for. */
	ENGINE_API static float Decode(float Encoded);
};
