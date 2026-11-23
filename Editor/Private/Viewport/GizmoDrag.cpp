#include "Viewport/GizmoDrag.h"

#include "Math/Ray.h"
#include "Viewport/GizmoSettings.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <vector>

namespace
{
	constexpr int32 AxisCount = 3;

	/**
	 * How far onwards along a ring, in lengths of the gizmo's axes, a second point is taken to see which way the
	 * ring runs in the picture, and the least that the two have to be apart there, in pixels, to say so.
	 */
	constexpr float RingProbeLength = 0.05f;
	constexpr float MinRingProbeLength = 1.0e-3f;

	/**
	 * How squarely the plane that holds an axis has to be able to face the camera for the mouse to point at it,
	 * as the sine of the angle between the axis and the line of sight: less, and the axis lies along that line.
	 */
	constexpr float MinPlaneFacing = 1.0e-3f;

	/** Returns a number as the nearest whole multiple of a step. */
	double RoundToStep(double Number, double Step)
	{
		return std::round(Number / Step) * Step;
	}

	/**
	 * Returns the factor that a drag multiplies a size by: in whole steps if sizes go in steps, and never less
	 * than a step, or than the least that a drag leaves of a size.
	 */
	float LimitScaleFactor(double Factor, const FGizmoSettings& Settings)
	{
		double Least = static_cast<double>(FGizmoDrag::MinScaleFactor);
		if (Settings.bSnapScale)
		{
			const double Step = static_cast<double>(Settings.ScaleSnap);
			Factor = RoundToStep(Factor, Step);
			Least = std::max(Least, Step);
		}
		return static_cast<float>(std::max(Factor, Least));
	}
}

FGizmoDrag::FGizmoDrag(
	const FTransformGizmo& InGizmo, EGizmoMode InMode, EGizmoHandle InHandle, const FVector2f& InStartPoint)
	: Gizmo(InGizmo)
	, Mode(InMode)
	, Handle(InHandle)
	, StartPoint(InStartPoint)
{
	// Whatever the handle is measured by is taken where the mouse took hold. A drag whose handle is not measured
	// by one of these has no use for it.
	StartAlongAxis = FindAlongAxis(StartPoint).value_or(0.0);
	StartInPlane = FindInPlane(StartPoint).value_or(Gizmo.GetPivot());
	RingDirection = FindRingDirection();
}

void FGizmoDrag::MoveTo(const FVector2f& PicturePoint, const FGizmoSettings& Settings)
{
	const std::optional<int32> AxisIndex = FTransformGizmo::GetAxisIndex(Handle);
	const FVector2f MovedInPicture = PicturePoint - StartPoint;
	switch (Mode)
	{
		case EGizmoMode::Translate:
			if (FTransformGizmo::IsAxisHandle(Handle))
			{
				if (const std::optional<double> AlongAxis = FindAlongAxis(PicturePoint))
				{
					const double Distance = *AlongAxis - StartAlongAxis;
					const double Stepped =
						Settings.bSnapLocation ? RoundToStep(Distance, Settings.LocationSnap) : Distance;
					Translation = FVector(Gizmo.GetAxis(*AxisIndex)) * Stepped;
				}
			}
			else if (FTransformGizmo::IsPlaneHandle(Handle))
			{
				if (const std::optional<FVector> InPlane = FindInPlane(PicturePoint))
				{
					// The point that the mouse took hold of has moved so far along each of the plane's two axes.
					const FVector Moved = *InPlane - StartInPlane;
					FVector Stepped(0.0);
					for (const int32 Next : {1, 2})
					{
						const FVector Axis(Gizmo.GetAxis((*AxisIndex + Next) % AxisCount));
						const double Distance = glm::dot(Moved, Axis);
						Stepped +=
							Axis * (Settings.bSnapLocation ? RoundToStep(Distance, Settings.LocationSnap) : Distance);
					}
					Translation = Stepped;
				}
			}
			break;
		case EGizmoMode::Rotate:
			if (FTransformGizmo::IsAxisHandle(Handle))
			{
				// Along the ring, the length of an axis in the picture is one radian.
				const double Degrees =
					static_cast<double>(glm::degrees(glm::dot(MovedInPicture, RingDirection) / Gizmo.GetAxisLength()));
				Angle = static_cast<float>(Settings.bSnapRotation
						? RoundToStep(Degrees, static_cast<double>(Settings.RotationSnap))
						: Degrees);
			}
			break;
		case EGizmoMode::Scale:
			if (FTransformGizmo::IsAxisHandle(Handle))
			{
				if (const std::optional<double> AlongAxis = FindAlongAxis(PicturePoint))
				{
					// The length of an axis is the whole of the size that the thing had along it.
					const double Factor = 1.0 + (*AlongAxis - StartAlongAxis) / Gizmo.GetWorldAxisLength();
					Scale = FVector3f(1.0f);
					Scale[*AxisIndex] = LimitScaleFactor(Factor, Settings);
				}
			}
			else if (Handle == EGizmoHandle::All)
			{
				// To the right and upwards is larger; the picture counts downwards.
				const double Factor =
					1.0 + static_cast<double>(MovedInPicture.x - MovedInPicture.y) / Gizmo.GetAxisLength();
				Scale = FVector3f(LimitScaleFactor(Factor, Settings));
			}
			break;
		default:
			break;
	}
}

FTransform FGizmoDrag::Apply(const FTransform& StartTransform) const
{
	FTransform Result = StartTransform;
	switch (Mode)
	{
		case EGizmoMode::Translate:
			Result.Location += Translation;
			break;
		case EGizmoMode::Rotate:
			// A drag that has not turned anything yet leaves a thing exactly where it was.
			if (FTransformGizmo::IsAxisHandle(Handle) && Angle != 0.0f)
			{
				// Around the gizmo's place: a thing that is somewhere else goes around that place as it turns.
				const std::optional<int32> AxisIndex = FTransformGizmo::GetAxisIndex(Handle);
				const FQuat Turn = glm::angleAxis(glm::radians(Angle), Gizmo.GetAxis(*AxisIndex));
				const FVector& Pivot = Gizmo.GetPivot();
				Result.Location = Pivot + glm::dquat(Turn) * (StartTransform.Location - Pivot);
				Result.Rotation = glm::normalize(Turn * StartTransform.Rotation);
			}
			break;
		case EGizmoMode::Scale:
			Result.Scale = StartTransform.Scale * Scale;
			break;
		default:
			break;
	}
	return Result;
}

std::optional<double> FGizmoDrag::FindAlongAxis(const FVector2f& PicturePoint) const
{
	const std::optional<int32> AxisIndex = FTransformGizmo::GetAxisIndex(Handle);
	if (!AxisIndex.has_value())
	{
		return std::nullopt;
	}

	// The mouse hardly ever points at the axis itself. It points at the plane that holds the axis and faces the
	// camera as squarely as such a plane can, and where it points there is as far along the axis as the mouse
	// is in the picture. An axis that lies along the line of sight has no such plane.
	const FVector3f& Axis = Gizmo.GetAxis(*AxisIndex);
	const FVector3f& ViewDirection = Gizmo.GetViewDirection();
	const FVector3f Facing = ViewDirection - Axis * glm::dot(ViewDirection, Axis);
	if (glm::length(Facing) < MinPlaneFacing)
	{
		return std::nullopt;
	}

	const FRay Ray = Gizmo.GetRay(PicturePoint);
	const std::optional<double> Distance = Ray.IntersectPlane(Gizmo.GetPivot(), glm::normalize(Facing));
	if (!Distance.has_value())
	{
		return std::nullopt;
	}
	return glm::dot(Ray.GetPoint(*Distance) - Gizmo.GetPivot(), FVector(Axis));
}

std::optional<FVector> FGizmoDrag::FindInPlane(const FVector2f& PicturePoint) const
{
	const std::optional<int32> AxisIndex = FTransformGizmo::GetAxisIndex(Handle);
	if (!AxisIndex.has_value())
	{
		return std::nullopt;
	}

	// The plane of a handle is the one that faces along the axis that the handle leaves out.
	const FRay Ray = Gizmo.GetRay(PicturePoint);
	const std::optional<double> Distance = Ray.IntersectPlane(Gizmo.GetPivot(), Gizmo.GetAxis(*AxisIndex));
	if (!Distance.has_value())
	{
		return std::nullopt;
	}
	return Ray.GetPoint(*Distance);
}

FVector2f FGizmoDrag::FindRingDirection() const
{
	const FVector2f AnyDirection(1.0f, 0.0f);
	const std::optional<int32> AxisIndex = FTransformGizmo::GetAxisIndex(Handle);
	if (Mode != EGizmoMode::Rotate || !FTransformGizmo::IsAxisHandle(Handle) || !AxisIndex.has_value())
	{
		return AnyDirection;
	}

	// The point of the ring that the mouse took hold of: the nearest of those that the ring is drawn through. Of
	// a ring that is seen from its edge, the near half and the far half lie on each other in the picture; where
	// two points are as near as makes no difference, the mouse has the one in front.
	const FVector3f& Axis = Gizmo.GetAxis(*AxisIndex);
	const FVector3f& First = Gizmo.GetAxis((*AxisIndex + 1) % AxisCount);
	const FVector3f& Second = Gizmo.GetAxis((*AxisIndex + 2) % AxisCount);
	const double Radius = Gizmo.GetWorldAxisLength();
	const float PieceAngle = 2.0f * std::numbers::pi_v<float> / static_cast<float>(FTransformGizmo::RingPieceCount);

	/** A point that the ring is drawn through. */
	struct FRingPoint
	{
		/** The direction from the gizmo's place to the point, of unit length. */
		FVector3f Outwards;

		/** How far the point is from where the mouse took hold, in pixels of the picture. */
		float Distance = 0.0f;

		/** How far it is behind the gizmo's place, as seen from the camera, in lengths of the gizmo's axes. */
		float Depth = 0.0f;
	};
	std::vector<FRingPoint> RingPoints;
	float NearestDistance = std::numeric_limits<float>::infinity();
	for (int32 Piece = 0; Piece < FTransformGizmo::RingPieceCount; ++Piece)
	{
		const float RingAngle = PieceAngle * static_cast<float>(Piece);
		const FVector3f Candidate = First * std::cos(RingAngle) + Second * std::sin(RingAngle);
		if (const std::optional<FVector2f> InPicture =
				Gizmo.FindInPicture(Gizmo.GetPivot() + FVector(Candidate) * Radius))
		{
			const float Distance = glm::length(*InPicture - StartPoint);
			RingPoints.push_back({Candidate, Distance, glm::dot(Candidate, Gizmo.GetViewDirection())});
			NearestDistance = std::min(NearestDistance, Distance);
		}
	}

	// As near as makes no difference: within half of what two points of the ring can be apart in the picture.
	const float SameDistance = Gizmo.GetAxisLength() * PieceAngle * 0.5f;
	FVector3f Outwards = First;
	float FrontDepth = std::numeric_limits<float>::infinity();
	for (const FRingPoint& RingPoint : RingPoints)
	{
		if (RingPoint.Distance <= NearestDistance + SameDistance && RingPoint.Depth < FrontDepth)
		{
			FrontDepth = RingPoint.Depth;
			Outwards = RingPoint.Outwards;
		}
	}

	// Turning a thing counter-clockwise around the axis takes that point of the ring this way, in the world and
	// so in the picture.
	const FVector3f Onwards = glm::cross(Axis, Outwards);
	const FVector OnRing = Gizmo.GetPivot() + FVector(Outwards) * Radius;
	const std::optional<FVector2f> From = Gizmo.FindInPicture(OnRing);
	const std::optional<FVector2f> To =
		Gizmo.FindInPicture(OnRing + FVector(Onwards) * (Radius * static_cast<double>(RingProbeLength)));
	if (From.has_value() && To.has_value() && glm::length(*To - *From) > MinRingProbeLength)
	{
		return glm::normalize(*To - *From);
	}

	// Where a ring is seen from its edge and runs straight at the camera, the picture has no direction for it.
	return AnyDirection;
}
