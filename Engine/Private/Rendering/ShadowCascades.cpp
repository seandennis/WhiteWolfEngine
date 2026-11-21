#include "Rendering/ShadowCascades.h"

#include "Math/Vector.h"
#include "Rendering/CameraView.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
	/**
	 * How the cascades share the shadow distance: cascade i ends at the fraction ((i + 1) / count) to this power.
	 * A power above one makes the near cascades short, where detail is needed.
	 */
	constexpr float SplitExponent = 2.5f;

	/** Each cascade's sphere is this much larger than its slice, so that a soft shadow edge has room at the border. */
	constexpr float RadiusMargin = 1.05f;
}

FShadowCascades FShadowCascades::Compute(
	const FCameraView& View, float AspectRatio, const FQuat& LightRotation, float ShadowDistance, int32 ShadowMapSize)
{
	WW_CHECKF(AspectRatio > 0.0f, "The aspect ratio of a picture must be greater than zero, not {}", AspectRatio);
	WW_CHECKF(ShadowDistance > 0.0f, "The shadow distance must be greater than zero, not {}", ShadowDistance);
	WW_CHECKF(ShadowMapSize > 0, "A shadow map must be at least one pixel wide, not {}", ShadowMapSize);

	// The light's axes, and the way the camera looks, along the world's axes.
	const FVector3f LightRight = LightRotation * FVector3f(1.0f, 0.0f, 0.0f);
	const FVector3f LightUp = LightRotation * FVector3f(0.0f, 1.0f, 0.0f);
	const FVector3f LightForward = LightRotation * FVector3f(0.0f, 0.0f, -1.0f);
	const FVector3f CameraForward = View.Rotation * FVector3f(0.0f, 0.0f, -1.0f);

	// How far from the view's axis a corner of the view is, per metre of distance.
	const float HalfHeight = std::tan(glm::radians(View.VerticalFieldOfView) * 0.5f);
	const float CornerSlopeSquared = HalfHeight * HalfHeight * (1.0f + AspectRatio * AspectRatio);

	const float CasterDistance = ShadowDistance * CasterDistanceFactor;

	FShadowCascades Result;
	float SliceNear = 0.0f;
	for (std::size_t Index = 0; Index < Result.Cascades.size(); ++Index)
	{
		const float Fraction = static_cast<float>(Index + 1) / static_cast<float>(CascadeCount);
		const float SliceFar = ShadowDistance * std::pow(Fraction, SplitExponent);

		// The smallest sphere around the slice of the view from SliceNear to SliceFar. Its centre is on the
		// view's axis, and for a wide view it is the middle of the slice's far end.
		const float CentreDistance = std::min(SliceFar, 0.5f * (SliceNear + SliceFar) * (1.0f + CornerSlopeSquared));
		const float ToFarCorner = std::sqrt(
			(SliceFar - CentreDistance) * (SliceFar - CentreDistance) + SliceFar * SliceFar * CornerSlopeSquared);
		const float ToNearCorner = std::sqrt(
			(SliceNear - CentreDistance) * (SliceNear - CentreDistance) + SliceNear * SliceNear * CornerSlopeSquared);
		const float Radius = std::max(ToFarCorner, ToNearCorner) * RadiusMargin;
		const double TexelSize = 2.0 * static_cast<double>(Radius) / static_cast<double>(ShadowMapSize);

		// Where the sphere's centre is across the light's view, measured in the world, and moved to a whole
		// number of shadow map pixels. As the camera moves, the map then moves by whole pixels, and what it
		// shows of the world does not shimmer. Only this step involves world coordinates, in double precision;
		// the result is taken relative to the camera again.
		const FVector CentreOffset = FVector(CameraForward * CentreDistance);
		const FVector WorldCentre = View.Location + CentreOffset;
		const double AcrossRight = std::floor(glm::dot(WorldCentre, FVector(LightRight)) / TexelSize) * TexelSize;
		const double AcrossUp = std::floor(glm::dot(WorldCentre, FVector(LightUp)) / TexelSize) * TexelSize;
		const float CentreRight = static_cast<float>(AcrossRight - glm::dot(View.Location, FVector(LightRight)));
		const float CentreUp = static_cast<float>(AcrossUp - glm::dot(View.Location, FVector(LightUp)));

		// Along the light's direction the map reaches from well before the sphere, so that things between the
		// light and the view cast their shadows into it, to the sphere's far side.
		const float CentreAlong = glm::dot(CameraForward * CentreDistance, LightForward);
		const float NearAlong = CentreAlong - Radius - CasterDistance;
		const float DepthRange = 2.0f * Radius + CasterDistance;

		// Each row of the matrix measures a camera-relative position along one of the light's axes.
		FMatrix4f Matrix(1.0f);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Matrix[Axis][0] = LightRight[Axis] / Radius;
			Matrix[Axis][1] = LightUp[Axis] / Radius;
			Matrix[Axis][2] = LightForward[Axis] / DepthRange;
		}
		Matrix[3] = glm::vec4(-CentreRight / Radius, -CentreUp / Radius, -NearAlong / DepthRange, 1.0f);

		FShadowCascade& Cascade = Result.Cascades[Index];
		Cascade.RelativeToShadow = Matrix;
		Cascade.FarDistance = SliceFar;
		Cascade.TexelSize = static_cast<float>(TexelSize);
		Cascade.DepthRange = DepthRange;

		SliceNear = SliceFar;
	}

	return Result;
}
