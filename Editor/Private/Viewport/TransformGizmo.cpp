#include "Viewport/TransformGizmo.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <utility>

namespace
{
	constexpr int32 AxisCount = 3;

	/** The handle along each axis, and the handle of the plane that leaves each axis out, by the index of the axis. */
	constexpr std::array<EGizmoHandle, AxisCount> AxisHandles = {EGizmoHandle::X, EGizmoHandle::Y, EGizmoHandle::Z};
	constexpr std::array<EGizmoHandle, AxisCount> PlaneHandles = {EGizmoHandle::YZ, EGizmoHandle::XZ, EGizmoHandle::XY};

	// The parts of the gizmo, measured in lengths of its axes.

	/** How long the head of an arrow is, and how far it reaches to each side of the arrow. */
	constexpr float ArrowHeadLength = 0.2f;
	constexpr float ArrowHeadHalfWidth = 0.07f;

	/** From where to where the square of a plane reaches along each of the plane's two axes. */
	constexpr float PlaneSquareNear = 0.25f;
	constexpr float PlaneSquareFar = 0.5f;

	/** Half the side of the box at the end of an axis, and of the box in the middle. */
	constexpr float AxisBoxHalfSize = 0.06f;
	constexpr float CenterBoxHalfSize = 0.09f;

	/**
	 * How nearly an axis may point along the line of sight and still have an arrow, as the cosine of the angle
	 * between the two: within some ten degrees, the arrow is a stub in the picture, and a pixel along it a long
	 * way in the world.
	 */
	constexpr float MaxAxisFacing = 0.985f;

	/**
	 * How nearly from its edge the square of a plane may be seen and still be there, as the cosine of the angle
	 * between the line of sight and the way the square faces: from flatter than some twelve degrees, it is a
	 * sliver.
	 */
	constexpr float MinPlaneFacing = 0.2f;

	/**
	 * How far behind the middle of the gizmo, as the camera sees it, a point of a ring may be and still be there,
	 * in lengths of the gizmo's axes. The half of a ring that is behind the middle is left out, so that it can be
	 * told which way a ring leans; this little more than half lets a ring that is seen squarely be whole.
	 */
	constexpr float MaxRingPointDepth = 0.05f;

	/**
	 * The pieces of a ring at whose beginnings the mouse can take hold of the ring without looking for it: the
	 * four that begin halfway between the two axes that the ring lies in.
	 */
	constexpr int32 RingHandleCount = 4;
	constexpr int32 FirstRingHandlePiece = FTransformGizmo::RingPieceCount / 8;
	constexpr int32 RingHandlePieceStep = FTransformGizmo::RingPieceCount / RingHandleCount;

	/**
	 * How far the point at which a handle is taken hold of may be from the handle's own shape, in pixels: it is on
	 * the shape, give or take what rounding makes of that.
	 */
	constexpr float HandlePointReach = 0.01f;

	const FVector3f UnturnedForward(0.0f, 0.0f, -1.0f);

	/**
	 * Returns where the ring around an axis is after so many of its pieces, in lengths of the gizmo's axes along
	 * each of them: on a circle in the plane of the two other axes, one length from the middle. After all of its
	 * pieces the ring is where it began.
	 */
	FVector3f GetRingPoint(int32 AxisIndex, int32 Piece)
	{
		const float Angle = 2.0f * std::numbers::pi_v<float> *
			static_cast<float>(Piece % FTransformGizmo::RingPieceCount) /
			static_cast<float>(FTransformGizmo::RingPieceCount);
		FVector3f AxisLengths(0.0f);
		AxisLengths[(AxisIndex + 1) % AxisCount] = std::cos(Angle);
		AxisLengths[(AxisIndex + 2) % AxisCount] = std::sin(Angle);
		return AxisLengths;
	}

	/** Returns the square around a point of the picture that reaches HalfSize to every side. */
	std::vector<FVector2f> MakeSquare(const FVector2f& Center, float HalfSize)
	{
		return {Center + FVector2f(-HalfSize, -HalfSize),
			Center + FVector2f(HalfSize, -HalfSize),
			Center + FVector2f(HalfSize, HalfSize),
			Center + FVector2f(-HalfSize, HalfSize)};
	}
}

FTransformGizmo::FTransformGizmo(const FCameraView& InView,
	const FVector2f& InPictureSize,
	const FVector& InPivot,
	const FQuat& Orientation,
	float InAxisLength)
	: View(InView)
	, PictureSize(InPictureSize)
	, Pivot(InPivot)
	, Axes{Orientation * FVector3f(1.0f, 0.0f, 0.0f),
		  Orientation * FVector3f(0.0f, 1.0f, 0.0f),
		  Orientation * FVector3f(0.0f, 0.0f, 1.0f)}
	, AxisLength(InAxisLength)
{
	WW_CHECKF(PictureSize.x > 0.0f && PictureSize.y > 0.0f,
		"A gizmo cannot be laid out for a picture of {} by {} pixels",
		PictureSize.x,
		PictureSize.y);
	WW_CHECKF(AxisLength > 0.0f, "The axes of a gizmo cannot be {} pixels long", AxisLength);

	// How far in front of the camera the gizmo is, along the way that the camera looks. The step from the world to
	// the camera is taken in double precision, like every other.
	const FVector3f FromCamera = FVector3f(Pivot - View.Location);
	const float Depth = glm::dot(FromCamera, View.Rotation * UnturnedForward);
	bIsVisible = Depth >= View.NearPlane;
	if (bIsVisible)
	{
		// At that depth the picture is so many metres high, and a pixel is its part of that.
		const double HalfHeight = std::tan(glm::radians(static_cast<double>(View.VerticalFieldOfView)) * 0.5);
		const double MetresPerPixel =
			2.0 * static_cast<double>(Depth) * HalfHeight / static_cast<double>(PictureSize.y);
		WorldAxisLength = static_cast<double>(AxisLength) * MetresPerPixel;
		ViewDirection = glm::normalize(FromCamera);
	}
}

const FVector3f& FTransformGizmo::GetAxis(int32 AxisIndex) const
{
	WW_CHECKF(AxisIndex >= 0 && AxisIndex < AxisCount, "A gizmo has the axes 0, 1 and 2, and no axis {}", AxisIndex);
	return Axes[static_cast<std::size_t>(AxisIndex)];
}

FRay FTransformGizmo::GetRay(const FVector2f& PicturePoint) const
{
	// The picture counts from its top left corner and downwards; the camera from the middle, and upwards.
	const FVector2f FromCorner = PicturePoint / PictureSize;
	return View.GetRay(
		FVector2f(FromCorner.x * 2.0f - 1.0f, 1.0f - FromCorner.y * 2.0f), PictureSize.x / PictureSize.y);
}

std::optional<FVector2f> FTransformGizmo::FindInPicture(const FVector& WorldPoint) const
{
	const std::optional<FVector2f> Projected = View.Project(WorldPoint, PictureSize.x / PictureSize.y);
	if (!Projected.has_value())
	{
		return std::nullopt;
	}
	return FVector2f((Projected->x + 1.0f) * 0.5f, (1.0f - Projected->y) * 0.5f) * PictureSize;
}

std::vector<FGizmoShape> FTransformGizmo::GetShapes(EGizmoMode Mode) const
{
	std::vector<FGizmoShape> Shapes;
	if (!bIsVisible)
	{
		return Shapes;
	}

	switch (Mode)
	{
		case EGizmoMode::Translate:
			AddTranslateShapes(Shapes);
			break;
		case EGizmoMode::Rotate:
			AddRotateShapes(Shapes);
			break;
		case EGizmoMode::Scale:
			AddScaleShapes(Shapes);
			break;
		default:
			break;
	}
	return Shapes;
}

EGizmoHandle FTransformGizmo::FindHandle(EGizmoMode Mode, const FVector2f& PicturePoint, float Reach) const
{
	// Of several shapes that are as near, the first counts: the shapes of a mode are in the order in which the
	// mouse should have them.
	EGizmoHandle Nearest = EGizmoHandle::None;
	float NearestDistance = std::numeric_limits<float>::infinity();
	for (const FGizmoShape& Shape : GetShapes(Mode))
	{
		const float Distance = Shape.GetDistanceTo(PicturePoint);
		if (Distance <= Reach && Distance < NearestDistance)
		{
			Nearest = Shape.Handle;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

std::optional<FVector2f> FTransformGizmo::FindHandlePoint(EGizmoMode Mode, EGizmoHandle Handle) const
{
	if (!bIsVisible)
	{
		return std::nullopt;
	}

	// Where the handle may be taken hold of, in lengths of the gizmo's axes along each of them.
	const std::optional<int32> AxisIndex = GetAxisIndex(Handle);
	std::vector<FVector3f> Places;
	switch (Mode)
	{
		case EGizmoMode::Translate:
			if (IsAxisHandle(Handle))
			{
				FVector3f InHead(0.0f);
				InHead[*AxisIndex] = 1.0f - ArrowHeadLength * 0.5f;
				Places.push_back(InHead);
			}
			else if (IsPlaneHandle(Handle))
			{
				FVector3f InSquare((PlaneSquareNear + PlaneSquareFar) * 0.5f);
				InSquare[*AxisIndex] = 0.0f;
				Places.push_back(InSquare);
			}
			break;
		case EGizmoMode::Rotate:
			if (IsAxisHandle(Handle))
			{
				for (int32 Index = 0; Index < RingHandleCount; ++Index)
				{
					Places.push_back(GetRingPoint(*AxisIndex, FirstRingHandlePiece + Index * RingHandlePieceStep));
				}
			}
			break;
		case EGizmoMode::Scale:
			if (IsAxisHandle(Handle))
			{
				FVector3f AtEnd(0.0f);
				AtEnd[*AxisIndex] = 1.0f;
				Places.push_back(AtEnd);
			}
			else if (Handle == EGizmoHandle::All)
			{
				Places.push_back(FVector3f(0.0f));
			}
			break;
		default:
			break;
	}

	// A place counts only if the mouse has this handle there: a handle that cannot be used has no shape to be
	// on, a ring is not there where it is behind the gizmo, and where another handle lies over this one, the
	// mouse has the other.
	for (const FVector3f& Place : Places)
	{
		const std::optional<FVector2f> Point = FindInPicture(GetPoint(Place));
		if (Point.has_value() && FindHandle(Mode, *Point, HandlePointReach) == Handle)
		{
			return Point;
		}
	}
	return std::nullopt;
}

std::optional<int32> FTransformGizmo::GetAxisIndex(EGizmoHandle Handle)
{
	switch (Handle)
	{
		case EGizmoHandle::X:
		case EGizmoHandle::YZ:
			return 0;
		case EGizmoHandle::Y:
		case EGizmoHandle::XZ:
			return 1;
		case EGizmoHandle::Z:
		case EGizmoHandle::XY:
			return 2;
		case EGizmoHandle::None:
		case EGizmoHandle::All:
		default:
			return std::nullopt;
	}
}

bool FTransformGizmo::IsAxisUsable(int32 AxisIndex) const
{
	return std::abs(glm::dot(GetAxis(AxisIndex), ViewDirection)) <= MaxAxisFacing;
}

bool FTransformGizmo::IsPlaneUsable(int32 AxisIndex) const
{
	return std::abs(glm::dot(GetAxis(AxisIndex), ViewDirection)) >= MinPlaneFacing;
}

bool FTransformGizmo::IsRingPointInFront(int32 AxisIndex, int32 Piece) const
{
	// How far along the line of sight the point is from the middle of the gizmo, in lengths of its axes.
	const FVector3f AxisLengths = GetRingPoint(AxisIndex, Piece);
	const FVector3f FromMiddle = Axes[0] * AxisLengths.x + Axes[1] * AxisLengths.y + Axes[2] * AxisLengths.z;
	return glm::dot(FromMiddle, ViewDirection) <= MaxRingPointDepth;
}

FVector FTransformGizmo::GetPoint(const FVector3f& AxisLengths) const
{
	const FVector3f FromPivot = Axes[0] * AxisLengths.x + Axes[1] * AxisLengths.y + Axes[2] * AxisLengths.z;
	return Pivot + FVector(FromPivot) * WorldAxisLength;
}

void FTransformGizmo::AddTranslateShapes(std::vector<FGizmoShape>& Shapes) const
{
	const std::optional<FVector2f> Center = FindInPicture(Pivot);
	for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
	{
		// An arrow: a line from the middle to its neck, and a head from there to its tip.
		FVector3f Along(0.0f);
		Along[AxisIndex] = 1.0f;
		const std::optional<FVector2f> Neck = FindInPicture(GetPoint(Along * (1.0f - ArrowHeadLength)));
		const std::optional<FVector2f> Tip = FindInPicture(GetPoint(Along));
		if (!IsAxisUsable(AxisIndex) || !Center.has_value() || !Neck.has_value() || !Tip.has_value())
		{
			continue;
		}

		const EGizmoHandle Handle = AxisHandles[static_cast<std::size_t>(AxisIndex)];
		Shapes.push_back({Handle, false, {*Center, *Neck}});

		// The head is as wide in the picture whichever way the arrow points.
		const FVector2f AlongPicture = *Tip - *Neck;
		const float HeadLength = glm::length(AlongPicture);
		if (HeadLength > 0.0f)
		{
			const FVector2f Across =
				FVector2f(-AlongPicture.y, AlongPicture.x) / HeadLength * (ArrowHeadHalfWidth * AxisLength);
			Shapes.push_back({Handle, true, {*Tip, *Neck + Across, *Neck - Across}});
		}
	}

	for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
	{
		// The square of the plane that leaves this axis out, between the two other axes.
		const int32 First = (AxisIndex + 1) % AxisCount;
		const int32 Second = (AxisIndex + 2) % AxisCount;
		FGizmoShape Square;
		Square.Handle = PlaneHandles[static_cast<std::size_t>(AxisIndex)];
		Square.bIsArea = true;
		for (const std::pair<float, float>& Corner : {std::pair(PlaneSquareNear, PlaneSquareNear),
				 std::pair(PlaneSquareFar, PlaneSquareNear),
				 std::pair(PlaneSquareFar, PlaneSquareFar),
				 std::pair(PlaneSquareNear, PlaneSquareFar)})
		{
			FVector3f AxisLengths(0.0f);
			AxisLengths[First] = Corner.first;
			AxisLengths[Second] = Corner.second;
			if (const std::optional<FVector2f> InPicture = FindInPicture(GetPoint(AxisLengths)))
			{
				Square.Points.push_back(*InPicture);
			}
		}

		constexpr std::size_t CornerCount = 4;
		if (IsPlaneUsable(AxisIndex) && Square.Points.size() == CornerCount)
		{
			Shapes.push_back(std::move(Square));
		}
	}
}

void FTransformGizmo::AddRotateShapes(std::vector<FGizmoShape>& Shapes) const
{
	for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
	{
		// A ring around this axis: a circle in the plane of the two others, of which the part is there that is
		// not behind the gizmo. That part is in one piece, and begins after a point that is not there.
		int32 FirstPiece = 0;
		int32 PointCount = 0;
		for (int32 Piece = 0; Piece < RingPieceCount; ++Piece)
		{
			if (IsRingPointInFront(AxisIndex, Piece))
			{
				++PointCount;
				if (!IsRingPointInFront(AxisIndex, Piece + RingPieceCount - 1))
				{
					FirstPiece = Piece;
				}
			}
		}

		// A ring that is all there is closed, by ending where it began.
		if (PointCount == RingPieceCount)
		{
			++PointCount;
		}

		FGizmoShape Ring;
		Ring.Handle = AxisHandles[static_cast<std::size_t>(AxisIndex)];
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			if (const std::optional<FVector2f> InPicture =
					FindInPicture(GetPoint(GetRingPoint(AxisIndex, FirstPiece + Index))))
			{
				Ring.Points.push_back(*InPicture);
			}
		}

		// A ring that reaches behind the camera's near plane is left out whole.
		if (Ring.Points.size() == static_cast<std::size_t>(PointCount))
		{
			Shapes.push_back(std::move(Ring));
		}
	}
}

void FTransformGizmo::AddScaleShapes(std::vector<FGizmoShape>& Shapes) const
{
	const std::optional<FVector2f> Center = FindInPicture(Pivot);
	if (!Center.has_value())
	{
		return;
	}

	// The box in the middle comes first: where the lines of the axes begin, the mouse is on the box.
	Shapes.push_back({EGizmoHandle::All, true, MakeSquare(*Center, CenterBoxHalfSize * AxisLength)});
	for (int32 AxisIndex = 0; AxisIndex < AxisCount; ++AxisIndex)
	{
		FVector3f Along(0.0f);
		Along[AxisIndex] = 1.0f;
		const std::optional<FVector2f> End = FindInPicture(GetPoint(Along));
		if (!IsAxisUsable(AxisIndex) || !End.has_value())
		{
			continue;
		}

		const EGizmoHandle Handle = AxisHandles[static_cast<std::size_t>(AxisIndex)];
		Shapes.push_back({Handle, false, {*Center, *End}});
		Shapes.push_back({Handle, true, MakeSquare(*End, AxisBoxHalfSize * AxisLength)});
	}
}
