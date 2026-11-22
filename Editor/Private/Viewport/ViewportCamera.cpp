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
}

bool FViewportCamera::IsValid() const
{
	const bool bIsFinite = std::isfinite(Location.x) && std::isfinite(Location.y) && std::isfinite(Location.z) &&
		std::isfinite(Yaw) && std::isfinite(Pitch) && std::isfinite(VerticalFieldOfView) && std::isfinite(Exposure);
	const bool bHasValidFieldOfView =
		VerticalFieldOfView >= FCameraView::MinFieldOfView && VerticalFieldOfView <= FCameraView::MaxFieldOfView;
	return bIsFinite && std::abs(Pitch) <= MaxPitch && bHasValidFieldOfView && Exposure > 0.0f;
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

bool FViewportCamera::LookAt(const FVector& Target)
{
	const FVector Offset = Target - Location;
	const double Distance = glm::length(Offset);
	if (!std::isfinite(Distance) || Distance <= 0.0)
	{
		return false;
	}

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

FCameraView FViewportCamera::GetView() const
{
	FCameraView View;
	View.Location = Location;
	View.Rotation = GetRotation();
	View.VerticalFieldOfView = VerticalFieldOfView;
	View.Exposure = Exposure;
	return View;
}
