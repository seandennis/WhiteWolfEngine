#include "Viewport/GizmoShape.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace
{
	/** Returns how far a point is from the straight piece between two points. */
	float GetDistanceToPiece(const FVector2f& Point, const FVector2f& From, const FVector2f& To)
	{
		const FVector2f Along = To - From;
		const float LengthSquared = glm::dot(Along, Along);
		if (LengthSquared <= 0.0f)
		{
			return glm::length(Point - From);
		}

		// The nearest point of the piece is where the point falls on it, or the end that it falls beyond.
		const float Part = std::clamp(glm::dot(Point - From, Along) / LengthSquared, 0.0f, 1.0f);
		return glm::length(Point - (From + Along * Part));
	}

	/** Returns on which side of the way from one point to another a third lies: above zero, below it, or zero on it. */
	float GetSide(const FVector2f& From, const FVector2f& To, const FVector2f& Point)
	{
		const FVector2f Along = To - From;
		const FVector2f Across = Point - From;
		return Along.x * Across.y - Along.y * Across.x;
	}
}

float FGizmoShape::GetDistanceTo(const FVector2f& Point) const
{
	if (Points.empty())
	{
		return std::numeric_limits<float>::infinity();
	}

	// An area is closed: after its last corner comes its first. A line ends at its last point.
	const std::size_t PieceCount = bIsArea ? Points.size() : Points.size() - 1;
	float Nearest = glm::length(Point - Points.front());
	bool bIsLeftOfAll = true;
	bool bIsRightOfAll = true;
	for (std::size_t Index = 0; Index < PieceCount; ++Index)
	{
		const FVector2f& From = Points[Index];
		const FVector2f& To = Points[(Index + 1) % Points.size()];
		Nearest = std::min(Nearest, GetDistanceToPiece(Point, From, To));

		const float Side = GetSide(From, To, Point);
		bIsLeftOfAll = bIsLeftOfAll && Side >= 0.0f;
		bIsRightOfAll = bIsRightOfAll && Side <= 0.0f;
	}

	// A point is inside an area that bulges nowhere inwards if it is on the same side of every edge, whichever way
	// round the corners go.
	const bool bIsInside = bIsArea && Points.size() >= 3 && (bIsLeftOfAll || bIsRightOfAll);
	return bIsInside ? 0.0f : Nearest;
}
