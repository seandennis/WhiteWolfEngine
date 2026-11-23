#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

/**
 * A box whose sides lie along the axes of the space it is in: the room that some points take up.
 *
 * A box begins empty and grows with every point that is added to it. An empty box contains nothing: its corners
 * mean nothing, and its centre, size and radius are zero. Use the names below, FBox and FBox3f.
 */
template <typename VectorType>
struct TBox
{
	/** The number type of the coordinates. */
	using FScalar = typename VectorType::value_type;

	/** The corner with the smallest coordinates and the one with the largest. Both zero while the box is empty. */
	VectorType Min = VectorType(0);
	VectorType Max = VectorType(0);

	/** True until a point has been added. */
	bool bIsEmpty = true;

	/** Makes the box large enough to contain a point. */
	void Add(const VectorType& Point)
	{
		if (bIsEmpty)
		{
			Min = Point;
			Max = Point;
			bIsEmpty = false;
		}
		else
		{
			Min = glm::min(Min, Point);
			Max = glm::max(Max, Point);
		}
	}

	/** Makes the box large enough to contain another box. An empty box adds nothing. */
	void Add(const TBox& Other)
	{
		if (!Other.bIsEmpty)
		{
			Add(Other.Min);
			Add(Other.Max);
		}
	}

	/** Returns the point in the middle of the box. */
	VectorType GetCenter() const
	{
		return (Min + Max) * static_cast<FScalar>(0.5);
	}

	/** Returns the lengths of the sides of the box. */
	VectorType GetSize() const
	{
		return Max - Min;
	}

	/** Returns how far the corners are from the centre: the radius of the smallest ball there that holds the box. */
	FScalar GetRadius() const
	{
		return glm::length(GetSize()) * static_cast<FScalar>(0.5);
	}

	bool operator==(const TBox& Other) const = default;
};

/** A box in world space, in double precision. */
using FBox = TBox<FVector>;

/** A box in the space of a mesh or another space of its own, in single precision. */
using FBox3f = TBox<FVector3f>;
