#include "Math/Intersection.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

std::optional<float> FIntersection::RayTriangle(
	const FVector3f& Origin, const FVector3f& Direction, const FVector3f& A, const FVector3f& B, const FVector3f& C)
{
	// The method of Moeller and Trumbore: the point where the ray meets the plane of the triangle is written as a
	// mix of the corners, and is inside the triangle if no corner's part is negative. Everything is left
	// multiplied by the determinant until the end, so that nothing is divided before it is known to be needed.
	const FVector3f EdgeToB = B - A;
	const FVector3f EdgeToC = C - A;
	const FVector3f AcrossC = glm::cross(Direction, EdgeToC);

	// How squarely the ray comes at the front, times twice the area: above zero for the front only, zero along
	// the plane and for a triangle without an area, below zero from behind. A number that is no number, as from
	// corners that are none, fails the comparison too.
	const float Determinant = glm::dot(EdgeToB, AcrossC);
	if (!(Determinant > 0.0f))
	{
		return std::nullopt;
	}

	const FVector3f FromA = Origin - A;
	const float PartOfB = glm::dot(FromA, AcrossC);
	if (PartOfB < 0.0f || PartOfB > Determinant)
	{
		return std::nullopt;
	}

	const FVector3f AcrossB = glm::cross(FromA, EdgeToB);
	const float PartOfC = glm::dot(Direction, AcrossB);
	if (PartOfC < 0.0f || PartOfB + PartOfC > Determinant)
	{
		return std::nullopt;
	}

	// Behind where the ray begins there is no ray. A determinant so small that the division runs over leaves
	// nothing to return either.
	const float Distance = glm::dot(EdgeToC, AcrossB) / Determinant;
	if (!(Distance >= 0.0f) || !std::isfinite(Distance))
	{
		return std::nullopt;
	}
	return Distance;
}

bool FIntersection::RayBox(const FVector3f& Origin, const FVector3f& Direction, const FBox3f& Box)
{
	if (Box.bIsEmpty)
	{
		return false;
	}

	// Between each pair of opposite sides the ray is inside for a stretch of its length. It meets the box if the
	// three stretches have a part in common that is not behind where the ray begins.
	float Entry = 0.0f;
	float Exit = std::numeric_limits<float>::infinity();
	for (glm::length_t Axis = 0; Axis < 3; ++Axis)
	{
		const float Start = Origin[Axis];
		const float Step = Direction[Axis];
		if (Step == 0.0f)
		{
			// Along this axis the ray stays where it begins: between the two sides for good, or never.
			if (Start < Box.Min[Axis] || Start > Box.Max[Axis])
			{
				return false;
			}
			continue;
		}

		float EntryOfAxis = (Box.Min[Axis] - Start) / Step;
		float ExitOfAxis = (Box.Max[Axis] - Start) / Step;
		if (EntryOfAxis > ExitOfAxis)
		{
			std::swap(EntryOfAxis, ExitOfAxis);
		}
		Entry = std::max(Entry, EntryOfAxis);
		Exit = std::min(Exit, ExitOfAxis);
		if (Entry > Exit)
		{
			return false;
		}
	}
	return true;
}
