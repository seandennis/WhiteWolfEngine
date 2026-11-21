#include "Math/Transform.h"

#include <cmath>

namespace
{
	/** Scale components with a magnitude at or below this are treated as zero when inverting a scale. */
	constexpr float SmallestInvertibleScale = 1.0e-8f;

	/** Returns 1 / Value, or zero when Value is too close to zero to invert. */
	float GetSafeReciprocal(float Value)
	{
		return std::abs(Value) <= SmallestInvertibleScale ? 0.0f : 1.0f / Value;
	}

	/** Returns the component-wise reciprocal of a scale, with zero for components that cannot be inverted. */
	FVector3f GetSafeScaleReciprocal(const FVector3f& Scale)
	{
		return FVector3f(GetSafeReciprocal(Scale.x), GetSafeReciprocal(Scale.y), GetSafeReciprocal(Scale.z));
	}
}

FVector FTransform::TransformPosition(const FVector& Position) const
{
	// The rotation is widened to double so that large positions keep their precision.
	const glm::dquat PreciseRotation(Rotation);
	return Location + PreciseRotation * (FVector(Scale) * Position);
}

FVector FTransform::InverseTransformPosition(const FVector& Position) const
{
	const glm::dquat PreciseInverseRotation = glm::conjugate(glm::dquat(Rotation));
	return FVector(GetSafeScaleReciprocal(Scale)) * (PreciseInverseRotation * (Position - Location));
}

FTransform FTransform::Combine(const FTransform& Parent, const FTransform& Child)
{
	FTransform Result;
	Result.Location = Parent.TransformPosition(Child.Location);
	Result.Rotation = glm::normalize(Parent.Rotation * Child.Rotation);
	Result.Scale = Parent.Scale * Child.Scale;
	return Result;
}

FTransform FTransform::GetRelativeTransform(const FTransform& Parent) const
{
	FTransform Result;
	Result.Location = Parent.InverseTransformPosition(Location);
	Result.Rotation = glm::normalize(glm::conjugate(Parent.Rotation) * Rotation);
	Result.Scale = Scale * GetSafeScaleReciprocal(Parent.Scale);
	return Result;
}
