#pragma once

#include "CoreMinimal.h"

#include "Math/Vector.h"

/** A rectangle of a user interface, in pixels from the top left corner of the surface it is drawn onto. */
struct FUiRectangle
{
	float Left = 0.0f;
	float Top = 0.0f;

	// The first column and row beyond the rectangle.

	float Right = 0.0f;
	float Bottom = 0.0f;

	float GetWidth() const
	{
		return Right - Left;
	}

	float GetHeight() const
	{
		return Bottom - Top;
	}

	/** Returns the middle of the rectangle, as a position that the mouse can be put at. */
	FVector2D GetCenter() const
	{
		return FVector2D((static_cast<double>(Left) + Right) * 0.5, (static_cast<double>(Top) + Bottom) * 0.5);
	}

	bool operator==(const FUiRectangle& Other) const = default;
};
