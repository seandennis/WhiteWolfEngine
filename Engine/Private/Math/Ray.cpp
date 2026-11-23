#include "Math/Ray.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace
{
	/**
	 * How close to zero the cosine of the angle between a ray and the way a plane faces may be before the ray
	 * counts as running along the plane: below this, what is divided by it is rounding and nothing else.
	 */
	constexpr double SmallestCosine = 1.0e-6;
}

std::optional<double> FRay::IntersectPlane(const FVector& PlanePoint, const FVector3f& PlaneNormal) const
{
	// How fast the ray comes towards the plane, and how far from the plane it begins, both along the normal.
	const FVector Normal(PlaneNormal);
	const double Approach = glm::dot(FVector(Direction), Normal);
	if (std::abs(Approach) < SmallestCosine)
	{
		return std::nullopt;
	}

	// A ray or a plane that is not made of finite numbers gives a distance that is no number, or one without
	// end: it is gone through nowhere.
	const double Distance = glm::dot(PlanePoint - Origin, Normal) / Approach;
	if (!std::isfinite(Distance) || Distance < 0.0)
	{
		return std::nullopt;
	}
	return Distance;
}
