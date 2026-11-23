#include "Rendering/CameraView.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

namespace
{
	/** How far the length of a rotation may be from one before it no longer counts as a unit quaternion. */
	constexpr float UnitLengthTolerance = 1.0e-3f;

	bool IsFinite(const FVector& Vector)
	{
		return std::isfinite(Vector.x) && std::isfinite(Vector.y) && std::isfinite(Vector.z);
	}

	bool IsFinite(const FQuat& Quat)
	{
		return std::isfinite(Quat.w) && std::isfinite(Quat.x) && std::isfinite(Quat.y) && std::isfinite(Quat.z);
	}
}

bool FCameraView::IsValid() const
{
	if (!IsFinite(Location) || !IsFinite(Rotation))
	{
		return false;
	}

	const bool bIsUnitRotation = std::abs(glm::length(Rotation) - 1.0f) <= UnitLengthTolerance;
	const bool bIsValidFieldOfView = VerticalFieldOfView >= MinFieldOfView && VerticalFieldOfView <= MaxFieldOfView;
	const bool bIsValidNearPlane = std::isfinite(NearPlane) && NearPlane > 0.0f;
	const bool bIsValidExposure = std::isfinite(Exposure) && Exposure > 0.0f;
	return bIsUnitRotation && bIsValidFieldOfView && bIsValidNearPlane && bIsValidExposure;
}

FMatrix4f FCameraView::GetCameraRelativeMatrix(const FTransform& WorldTransform) const
{
	// The subtraction is the one step that must happen in double precision: both locations may be enormous, but
	// their difference is small enough for single precision wherever the object is close enough to be seen well.
	const FVector3f Offset = FVector3f(WorldTransform.Location - Location);

	FMatrix4f Matrix = glm::mat4_cast(WorldTransform.Rotation);
	Matrix[0] *= WorldTransform.Scale.x;
	Matrix[1] *= WorldTransform.Scale.y;
	Matrix[2] *= WorldTransform.Scale.z;
	Matrix[3] = glm::vec4(Offset, 1.0f);
	return Matrix;
}

FMatrix4f FCameraView::GetViewMatrix() const
{
	// The inverse of a rotation is its conjugate.
	return glm::mat4_cast(glm::conjugate(Rotation));
}

FMatrix4f FCameraView::GetProjectionMatrix(float AspectRatio) const
{
	WW_CHECKF(AspectRatio > 0.0f, "The aspect ratio of a picture must be greater than zero, not {}", AspectRatio);

	const float FocalLength = 1.0f / std::tan(glm::radians(VerticalFieldOfView) * 0.5f);

	// A point at view-space depth -Distance gets w = Distance and z = NearPlane, so its depth after the division
	// by w is NearPlane / Distance: 1 at the near plane and 0 at infinity.
	FMatrix4f Matrix(0.0f);
	Matrix[0][0] = FocalLength / AspectRatio;
	Matrix[1][1] = FocalLength;
	Matrix[2][3] = -1.0f;
	Matrix[3][2] = NearPlane;
	return Matrix;
}

FRay FCameraView::GetRay(const FVector2f& PicturePoint, float AspectRatio) const
{
	WW_CHECKF(AspectRatio > 0.0f, "The aspect ratio of a picture must be greater than zero, not {}", AspectRatio);

	// A metre in front of the camera, the picture is as high as twice the tangent of half the field of view. The
	// point of the picture is seen where the ray crosses that plane.
	const float HalfHeight = std::tan(glm::radians(VerticalFieldOfView) * 0.5f);
	const FVector3f InViewSpace(PicturePoint.x * HalfHeight * AspectRatio, PicturePoint.y * HalfHeight, -1.0f);
	const FVector3f AlongWorldAxes = Rotation * InViewSpace;

	// The same offset, shortened to the distance of the near plane, is where the ray crosses that: nothing closer
	// to the camera is drawn.
	FRay Ray;
	Ray.Origin = Location + FVector(AlongWorldAxes) * static_cast<double>(NearPlane);
	Ray.Direction = glm::normalize(AlongWorldAxes);
	return Ray;
}

std::optional<FVector2f> FCameraView::Project(const FVector& WorldPoint, float AspectRatio) const
{
	WW_CHECKF(AspectRatio > 0.0f, "The aspect ratio of a picture must be greater than zero, not {}", AspectRatio);

	// From the camera, as everything is that is drawn: the subtraction in double precision, the rest in single.
	const FVector3f InViewSpace = glm::conjugate(Rotation) * FVector3f(WorldPoint - Location);
	const float Depth = -InViewSpace.z;
	if (!(Depth >= NearPlane))
	{
		return std::nullopt;
	}

	// At that depth the picture is this high and wide, measured from its middle.
	const float HalfHeight = std::tan(glm::radians(VerticalFieldOfView) * 0.5f) * Depth;
	return FVector2f(InViewSpace.x / (HalfHeight * AspectRatio), InViewSpace.y / HalfHeight);
}
