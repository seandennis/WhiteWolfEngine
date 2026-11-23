#include "Viewport/ViewportCamera.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace
{
	const FVector3f WorldUp(0.0f, 1.0f, 0.0f);
	const FVector3f WorldRight(1.0f, 0.0f, 0.0f);

	// A camera that is not turned looks along -Z, as every object does (D-028).
	const FVector3f UnturnedForward(0.0f, 0.0f, -1.0f);

	constexpr float FullTurn = 360.0f;
}

bool FViewportCamera::IsValid() const
{
	const bool bIsFinite = std::isfinite(Location.x) && std::isfinite(Location.y) && std::isfinite(Location.z) &&
		std::isfinite(Yaw) && std::isfinite(Pitch) && std::isfinite(VerticalFieldOfView) && std::isfinite(Exposure) &&
		std::isfinite(FocusDistance) && std::isfinite(FlySpeed);
	const bool bHasValidFieldOfView =
		VerticalFieldOfView >= FCameraView::MinFieldOfView && VerticalFieldOfView <= FCameraView::MaxFieldOfView;
	const bool bCanBeSteered = FocusDistance >= MinFocusDistance && FlySpeed >= MinFlySpeed && FlySpeed <= MaxFlySpeed;
	return bIsFinite && std::abs(Pitch) <= MaxPitch && bHasValidFieldOfView && Exposure > 0.0f && bCanBeSteered;
}

FQuat FViewportCamera::GetRotation() const
{
	// Turned first, then tilted around the axis that the turn has given the camera as its right.
	const FQuat Turn = glm::angleAxis(glm::radians(Yaw), WorldUp);
	const FQuat Tilt = glm::angleAxis(glm::radians(Pitch), WorldRight);
	return glm::normalize(Turn * Tilt);
}

FVector3f FViewportCamera::GetForward() const
{
	return GetRotation() * UnturnedForward;
}

FVector3f FViewportCamera::GetRight() const
{
	return GetRotation() * WorldRight;
}

FVector3f FViewportCamera::GetUp() const
{
	return GetRotation() * WorldUp;
}

FVector FViewportCamera::GetFocusPoint() const
{
	return Location + FVector(GetForward()) * FocusDistance;
}

bool FViewportCamera::LookAt(const FVector& Target)
{
	const FVector Offset = Target - Location;
	const double Distance = glm::length(Offset);
	if (!std::isfinite(Distance) || Distance <= 0.0)
	{
		return false;
	}
	FocusDistance = std::max(Distance, MinFocusDistance);

	const FVector Direction = Offset / Distance;
	const double Steepness = glm::degrees(std::asin(std::clamp(Direction.y, -1.0, 1.0)));
	Pitch = static_cast<float>(std::clamp(Steepness, -static_cast<double>(MaxPitch), static_cast<double>(MaxPitch)));

	// Straight up or down there is no way that is ahead, and the camera stays turned as it was.
	const bool bHasHeading = Direction.x != 0.0 || Direction.z != 0.0;
	if (bHasHeading)
	{
		Yaw = static_cast<float>(glm::degrees(std::atan2(-Direction.x, -Direction.z)));
	}
	return true;
}

void FViewportCamera::Turn(float YawDegrees, float PitchDegrees)
{
	// A full turn more or less is the same direction: the yaw is kept where it can be read.
	Yaw = std::remainder(Yaw + YawDegrees, FullTurn);
	Pitch = std::clamp(Pitch + PitchDegrees, -MaxPitch, MaxPitch);
}

void FViewportCamera::Fly(double Right, double Up, double Forward)
{
	Location += FVector(GetRight()) * Right + FVector(WorldUp) * Up + FVector(GetForward()) * Forward;
}

void FViewportCamera::Orbit(float YawDegrees, float PitchDegrees)
{
	const FVector FocusPoint = GetFocusPoint();
	Turn(YawDegrees, PitchDegrees);
	Location = FocusPoint - FVector(GetForward()) * FocusDistance;
}

void FViewportCamera::Dolly(double Distance)
{
	const double NewFocusDistance = std::max(FocusDistance - Distance, MinFocusDistance);
	Location += FVector(GetForward()) * (FocusDistance - NewFocusDistance);
	FocusDistance = NewFocusDistance;
}

void FViewportCamera::Pan(double Right, double Up)
{
	Location += FVector(GetRight()) * Right + FVector(GetUp()) * Up;
}

bool FViewportCamera::Frame(const FVector& Center, double Radius)
{
	const bool bIsBall = std::isfinite(Center.x) && std::isfinite(Center.y) && std::isfinite(Center.z) &&
		std::isfinite(Radius) && Radius > 0.0;
	if (!bIsBall)
	{
		return false;
	}

	// From this far, the ball just fills the picture from top to bottom; a picture is at least as wide as high.
	const double HalfAngle = glm::radians(static_cast<double>(VerticalFieldOfView)) * 0.5;
	const double Distance = std::max(Radius / std::sin(HalfAngle) * FrameMargin, MinFocusDistance);
	Location = Center - FVector(GetForward()) * Distance;
	FocusDistance = Distance;
	return true;
}

double FViewportCamera::GetPixelSizeAtFocus(int32 PictureHeight) const
{
	WW_CHECKF(PictureHeight > 0, "A picture cannot be {} pixels high", PictureHeight);
	const double HalfAngle = glm::radians(static_cast<double>(VerticalFieldOfView)) * 0.5;
	return 2.0 * FocusDistance * std::tan(HalfAngle) / static_cast<double>(PictureHeight);
}
FCameraView FViewportCamera::GetView() const
{
	FCameraView View;
	View.Location = Location;
	View.Rotation = GetRotation();
	View.VerticalFieldOfView = VerticalFieldOfView;
	View.Exposure = Exposure;
	return View;
}
