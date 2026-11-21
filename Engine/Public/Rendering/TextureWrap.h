#pragma once

#include "CoreMinimal.h"

/** What a texture shows outside the range from 0 to 1 of a texture coordinate. */
enum class ETextureWrap : uint8
{
	/** The texture repeats, like tiles. */
	Repeat,

	/** The texture repeats, and every other repetition is mirrored, so that the edges always match. */
	MirroredRepeat,

	/** The pixels at the edge of the texture continue outwards. */
	ClampToEdge,
};
