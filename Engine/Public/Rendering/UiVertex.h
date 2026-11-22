#pragma once

#include "CoreMinimal.h"

#include "Math/Color.h"

/** One corner of a triangle of a user interface; see FUiDrawList. */
struct FUiVertex
{
	/** Where the corner is, in pixels from the top left corner of the surface that is drawn on. */
	FVector2f Position = FVector2f(0.0f);

	/** Where in the texture the corner is: (0, 0) is the top left corner of the picture and (1, 1) the bottom right. */
	FVector2f TextureCoordinate = FVector2f(0.0f);

	/**
	 * The colour of the corner, which the texture multiplies. sRGB-encoded, as colours are picked and stored; its
	 * alpha says how much of what lies behind is covered.
	 */
	FColor Color = FColor(255, 255, 255, 255);
};
