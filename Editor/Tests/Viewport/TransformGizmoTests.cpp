#include "Viewport/TransformGizmo.h"

#include "Math/Ray.h"
#include "TestSupport/ScopedLogCapture.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <vector>

// Most of these tests look at a gizmo from straight in front, in numbers that can be done in the head: a camera
// where the world begins, looking along -Z with 90 degrees from the top of its picture to the bottom, and a
// gizmo ten metres ahead. In a picture of 200 pixels a side, a pixel is then ten centimetres at the gizmo, whose
// middle is at (100, 100), and its axes of 50 pixels are five metres long: X to the right, Y upwards, and Z
// straight at the camera.

namespace
{
	const FVector2f PictureSize(200.0f, 200.0f);
	const FVector FrontPivot(0.0, 0.0, -10.0);
	constexpr float AxisLength = 50.0f;
	const FQuat NoRotation(1.0f, 0.0f, 0.0f, 0.0f);

	FCameraView MakeFrontView()
	{
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;
		return View;
	}

	FTransformGizmo MakeFrontGizmo(const FQuat& Orientation = NoRotation)
	{
		return FTransformGizmo(MakeFrontView(), PictureSize, FrontPivot, Orientation, AxisLength);
	}

	bool IsNear(const FVector2f& Actual, const FVector2f& Expected, float Tolerance = 0.01f)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	/** Returns the shapes of a mode that belong to a handle, in the order they are in. */
	std::vector<FGizmoShape> GetShapesOf(const FTransformGizmo& Gizmo, EGizmoMode Mode, EGizmoHandle Handle)
	{
		std::vector<FGizmoShape> Found;
		for (const FGizmoShape& Shape : Gizmo.GetShapes(Mode))
		{
			if (Shape.Handle == Handle)
			{
				Found.push_back(Shape);
			}
		}
		return Found;
	}
}

TEST_SUITE("FTransformGizmo")
{
	TEST_CASE("A gizmo is as long in the world as its length in the picture is at its distance")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		CHECK(Gizmo.IsVisible());
		CHECK(Gizmo.GetPivot() == FrontPivot);
		CHECK(Gizmo.GetAxisLength() == AxisLength);
		CHECK(Gizmo.GetWorldAxisLength() == doctest::Approx(5.0));
		CHECK(IsNear(FVector2f(Gizmo.GetViewDirection().x, Gizmo.GetViewDirection().z), FVector2f(0.0f, -1.0f)));

		// Twice as far away it is twice as long, to look the same; in a picture twice as high, half as long.
		const FTransformGizmo Farther(MakeFrontView(), PictureSize, FVector(0.0, 0.0, -20.0), NoRotation, AxisLength);
		CHECK(Farther.GetWorldAxisLength() == doctest::Approx(10.0));
		const FTransformGizmo Finer(MakeFrontView(), FVector2f(400.0f, 400.0f), FrontPivot, NoRotation, AxisLength);
		CHECK(Finer.GetWorldAxisLength() == doctest::Approx(2.5));
	}

	TEST_CASE("The axes of a gizmo are those of the world, or turned as the gizmo is")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();
		CHECK(Gizmo.GetAxis(0) == FVector3f(1.0f, 0.0f, 0.0f));
		CHECK(Gizmo.GetAxis(1) == FVector3f(0.0f, 1.0f, 0.0f));
		CHECK(Gizmo.GetAxis(2) == FVector3f(0.0f, 0.0f, 1.0f));

		// A quarter turn around Z takes X to where Y was.
		const FTransformGizmo Turned = MakeFrontGizmo(glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 0.0f, 1.0f)));
		CHECK(glm::length(Turned.GetAxis(0) - FVector3f(0.0f, 1.0f, 0.0f)) < 1.0e-6f);
		CHECK(glm::length(Turned.GetAxis(1) - FVector3f(-1.0f, 0.0f, 0.0f)) < 1.0e-6f);
		CHECK(glm::length(Turned.GetAxis(2) - FVector3f(0.0f, 0.0f, 1.0f)) < 1.0e-6f);
	}

	TEST_CASE("The axes of a gizmo are of unit length, whatever the length of the rotation that turns them")
	{
		// A quarter turn around Y that is not of unit length, as a scene file or a command may give an entity, and
		// the same turn as a unit quaternion: X points away from the camera, and Z to the right.
		const FTransformGizmo Gizmo = MakeFrontGizmo(FQuat(1.0f, 0.0f, 1.0f, 0.0f));
		const FTransformGizmo Unit = MakeFrontGizmo(glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 1.0f, 0.0f)));

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			CAPTURE(AxisIndex);
			CHECK(glm::length(Gizmo.GetAxis(AxisIndex)) == doctest::Approx(1.0f));
			CHECK(glm::length(Gizmo.GetAxis(AxisIndex) - Unit.GetAxis(AxisIndex)) < 1.0e-6f);
		}
		CHECK(glm::length(Gizmo.GetAxis(0) - FVector3f(0.0f, 0.0f, -1.0f)) < 1.0e-6f);
		CHECK(glm::length(Gizmo.GetAxis(2) - FVector3f(1.0f, 0.0f, 0.0f)) < 1.0e-6f);

		// A rotation of no length at all turns nothing.
		const FTransformGizmo Unturned = MakeFrontGizmo(FQuat(0.0f, 0.0f, 0.0f, 0.0f));
		CHECK(Unturned.GetAxis(0) == FVector3f(1.0f, 0.0f, 0.0f));
		CHECK(Unturned.GetAxis(1) == FVector3f(0.0f, 1.0f, 0.0f));
		CHECK(Unturned.GetAxis(2) == FVector3f(0.0f, 0.0f, 1.0f));
	}

	TEST_CASE("An axis that a gizmo has not is a fatal error")
	{
		const FScopedLogCapture Capture;
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		CHECK_THROWS_AS(Gizmo.GetAxis(-1), FFatalErrorException);
		CHECK_THROWS_AS(Gizmo.GetAxis(3), FFatalErrorException);
		CHECK_THROWS_AS(Gizmo.GetRingPointDirection(-1, 0), FFatalErrorException);
		CHECK_THROWS_AS(Gizmo.IsRingPointInFront(3, 0), FFatalErrorException);
	}

	TEST_CASE("A gizmo cannot be laid out for a picture without a size, or with axes of no length")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(FTransformGizmo(MakeFrontView(), FVector2f(0.0f, 200.0f), FrontPivot, NoRotation, AxisLength),
			FFatalErrorException);
		CHECK_THROWS_AS(FTransformGizmo(MakeFrontView(), FVector2f(200.0f, -1.0f), FrontPivot, NoRotation, AxisLength),
			FFatalErrorException);
		CHECK_THROWS_AS(
			FTransformGizmo(MakeFrontView(), PictureSize, FrontPivot, NoRotation, 0.0f), FFatalErrorException);
	}

	TEST_CASE("Points of the world are found in the picture, and points of the picture seen along rays")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		// The middle, and a point two metres to the right and one up from it.
		const std::optional<FVector2f> Middle = Gizmo.FindInPicture(FrontPivot);
		REQUIRE(Middle.has_value());
		CHECK(IsNear(*Middle, FVector2f(100.0f, 100.0f)));
		const std::optional<FVector2f> Beside = Gizmo.FindInPicture(FrontPivot + FVector(2.0, 1.0, 0.0));
		REQUIRE(Beside.has_value());
		CHECK(IsNear(*Beside, FVector2f(120.0f, 90.0f)));
		CHECK_FALSE(Gizmo.FindInPicture(FVector(0.0, 0.0, 5.0)).has_value());

		// The ray through a point of the picture leads to what is drawn at that point.
		const FRay Ray = Gizmo.GetRay(FVector2f(37.0f, 150.0f));
		const std::optional<FVector2f> Back = Gizmo.FindInPicture(Ray.GetPoint(12.0));
		REQUIRE(Back.has_value());
		CHECK(IsNear(*Back, FVector2f(37.0f, 150.0f)));
	}

	TEST_CASE("To move things, a gizmo has an arrow along each axis and a square between each two")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		// The arrow along X: a line from the middle to its neck, and a head from there to its tip.
		const std::vector<FGizmoShape> ArrowX = GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::X);
		REQUIRE(ArrowX.size() == 2);
		CHECK_FALSE(ArrowX[0].bIsArea);
		REQUIRE(ArrowX[0].Points.size() == 2);
		CHECK(IsNear(ArrowX[0].Points[0], FVector2f(100.0f, 100.0f)));
		CHECK(IsNear(ArrowX[0].Points[1], FVector2f(140.0f, 100.0f)));
		CHECK(ArrowX[1].bIsArea);
		REQUIRE(ArrowX[1].Points.size() == 3);
		CHECK(IsNear(ArrowX[1].Points[0], FVector2f(150.0f, 100.0f)));
		CHECK(IsNear(ArrowX[1].Points[1], FVector2f(140.0f, 103.5f)));
		CHECK(IsNear(ArrowX[1].Points[2], FVector2f(140.0f, 96.5f)));

		// Up in the world is up in the picture, which counts downwards.
		const std::vector<FGizmoShape> ArrowY = GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::Y);
		REQUIRE(ArrowY.size() == 2);
		CHECK(IsNear(ArrowY[0].Points[1], FVector2f(100.0f, 60.0f)));
		CHECK(IsNear(ArrowY[1].Points[0], FVector2f(100.0f, 50.0f)));

		// The square of the plane of X and Y, from a quarter to a half of the axes' length along each.
		const std::vector<FGizmoShape> Square = GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::XY);
		REQUIRE(Square.size() == 1);
		CHECK(Square[0].bIsArea);
		REQUIRE(Square[0].Points.size() == 4);
		CHECK(IsNear(Square[0].Points[0], FVector2f(112.5f, 87.5f)));
		CHECK(IsNear(Square[0].Points[1], FVector2f(125.0f, 87.5f)));
		CHECK(IsNear(Square[0].Points[2], FVector2f(125.0f, 75.0f)));
		CHECK(IsNear(Square[0].Points[3], FVector2f(112.5f, 75.0f)));
	}

	TEST_CASE("A handle that cannot be used from where the camera is has no shape")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		// The arrow along Z points straight at the camera, and the squares that Z is in are seen from their edge.
		CHECK(GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::Z).empty());
		CHECK(GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::YZ).empty());
		CHECK(GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::XZ).empty());
		CHECK(GetShapesOf(Gizmo, EGizmoMode::Scale, EGizmoHandle::Z).empty());
		CHECK(Gizmo.GetShapes(EGizmoMode::Translate).size() == 5);

		// From the side and above, as the editor's camera first looks, every handle is there.
		const FViewportCamera Camera;
		const FTransformGizmo Seen(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), NoRotation, AxisLength);
		REQUIRE(Seen.IsVisible());
		for (const EGizmoHandle Handle : {EGizmoHandle::X, EGizmoHandle::Y, EGizmoHandle::Z})
		{
			CHECK(GetShapesOf(Seen, EGizmoMode::Translate, Handle).size() == 2);
			CHECK(GetShapesOf(Seen, EGizmoMode::Scale, Handle).size() == 2);
			CHECK(GetShapesOf(Seen, EGizmoMode::Rotate, Handle).size() == 1);
		}
		for (const EGizmoHandle Handle : {EGizmoHandle::YZ, EGizmoHandle::XZ, EGizmoHandle::XY})
		{
			CHECK(GetShapesOf(Seen, EGizmoMode::Translate, Handle).size() == 1);
		}
	}

	TEST_CASE("The arrows of a gizmo that is seen at a slant end where its axes do in the picture")
	{
		const FViewportCamera Camera;
		const FTransformGizmo Gizmo(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), NoRotation, AxisLength);
		const std::optional<FVector2f> Middle = Gizmo.FindInPicture(Gizmo.GetPivot());
		REQUIRE(Middle.has_value());

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const EGizmoHandle Handle = AxisIndex == 0 ? EGizmoHandle::X
				: AxisIndex == 1                       ? EGizmoHandle::Y
													   : EGizmoHandle::Z;
			const std::vector<FGizmoShape> Arrow = GetShapesOf(Gizmo, EGizmoMode::Translate, Handle);
			REQUIRE(Arrow.size() == 2);
			const std::optional<FVector2f> End =
				Gizmo.FindInPicture(Gizmo.GetPivot() + FVector(Gizmo.GetAxis(AxisIndex)) * Gizmo.GetWorldAxisLength());
			REQUIRE(End.has_value());

			CHECK(IsNear(Arrow[0].Points[0], *Middle));
			CHECK(IsNear(Arrow[1].Points[0], *End));
			// An axis that leans towards the camera or away from it is shorter in the picture, and never longer
			// than it would be seen squarely, give or take what being nearer adds.
			const float Length = glm::length(*End - *Middle);
			CHECK(Length > AxisLength * 0.2f);
			CHECK(Length < AxisLength * 1.3f);
		}
	}

	TEST_CASE("To turn things, a gizmo has a ring around each axis")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();
		const std::vector<FGizmoShape> Rings = Gizmo.GetShapes(EGizmoMode::Rotate);
		REQUIRE(Rings.size() == 3);

		// The ring around Z is seen squarely: a circle of the axes' length around the middle, closed.
		const std::vector<FGizmoShape> RingZ = GetShapesOf(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Z);
		REQUIRE(RingZ.size() == 1);
		CHECK_FALSE(RingZ[0].bIsArea);
		REQUIRE(RingZ[0].Points.size() == static_cast<std::size_t>(FTransformGizmo::RingPieceCount) + 1);
		for (const FVector2f& Point : RingZ[0].Points)
		{
			CHECK(glm::length(Point - FVector2f(100.0f, 100.0f)) == doctest::Approx(50.0f).epsilon(1.0e-4));
		}
		CHECK(IsNear(RingZ[0].Points.front(), RingZ[0].Points.back()));
		CHECK(IsNear(RingZ[0].Points.front(), FVector2f(150.0f, 100.0f)));

		// The rings around X and Y are seen from their edges: an upright line and a level one. Of each, the half
		// is there that is towards the camera: from one end of the line, through the middle of the gizmo, where
		// the ring is nearest to the camera, to the other end.
		const std::vector<FGizmoShape> RingX = GetShapesOf(Gizmo, EGizmoMode::Rotate, EGizmoHandle::X);
		REQUIRE(RingX.size() == 1);
		REQUIRE(RingX[0].Points.size() == static_cast<std::size_t>(FTransformGizmo::RingPieceCount) / 2 + 1);
		for (const FVector2f& Point : RingX[0].Points)
		{
			CHECK(Point.x == doctest::Approx(100.0f));
		}
		CHECK(IsNear(RingX[0].Points.front(), FVector2f(100.0f, 50.0f)));
		CHECK(IsNear(
			RingX[0].Points[static_cast<std::size_t>(FTransformGizmo::RingPieceCount) / 4], FVector2f(100.0f, 100.0f)));
		CHECK(IsNear(RingX[0].Points.back(), FVector2f(100.0f, 150.0f)));

		const std::vector<FGizmoShape> RingY = GetShapesOf(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Y);
		REQUIRE(RingY.size() == 1);
		REQUIRE(RingY[0].Points.size() == static_cast<std::size_t>(FTransformGizmo::RingPieceCount) / 2 + 1);
		for (const FVector2f& Point : RingY[0].Points)
		{
			CHECK(Point.y == doctest::Approx(100.0f));
		}
	}

	TEST_CASE("The points of a ring go around its axis from the next axis on, and those in front are there")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();
		constexpr int32 Quarter = FTransformGizmo::RingPieceCount / 4;

		// Around Y the ring begins at Z, which points at the camera, and goes on to X, to the right: the way that a
		// thing turns counter-clockwise as seen from above.
		CHECK(glm::length(Gizmo.GetRingPointDirection(1, 0) - FVector3f(0.0f, 0.0f, 1.0f)) < 1.0e-6f);
		CHECK(glm::length(Gizmo.GetRingPointDirection(1, Quarter) - FVector3f(1.0f, 0.0f, 0.0f)) < 1.0e-6f);
		CHECK(glm::length(Gizmo.GetRingPointDirection(1, 2 * Quarter) - FVector3f(0.0f, 0.0f, -1.0f)) < 1.0e-6f);
		// After its last piece a ring is where it began.
		CHECK(glm::length(Gizmo.GetRingPointDirection(1, FTransformGizmo::RingPieceCount) -
				  Gizmo.GetRingPointDirection(1, 0)) < 1.0e-6f);

		// The half towards the camera is in front, with the two points beside the middle that are neither in front
		// nor behind; the far half is not.
		CHECK(Gizmo.IsRingPointInFront(1, 0));
		CHECK(Gizmo.IsRingPointInFront(1, Quarter));
		CHECK(Gizmo.IsRingPointInFront(1, 3 * Quarter));
		CHECK_FALSE(Gizmo.IsRingPointInFront(1, Quarter + 1));
		CHECK_FALSE(Gizmo.IsRingPointInFront(1, 2 * Quarter));
		CHECK_FALSE(Gizmo.IsRingPointInFront(1, 3 * Quarter - 1));

		// The ring is drawn through the points in front, and through no others.
		const std::vector<FGizmoShape> Rings = GetShapesOf(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Y);
		REQUIRE(Rings.size() == 1);
		std::size_t FrontCount = 0;
		for (int32 Piece = 0; Piece < FTransformGizmo::RingPieceCount; ++Piece)
		{
			if (!Gizmo.IsRingPointInFront(1, Piece))
			{
				continue;
			}

			++FrontCount;
			const std::optional<FVector2f> InPicture = Gizmo.FindInPicture(
				Gizmo.GetPivot() + FVector(Gizmo.GetRingPointDirection(1, Piece)) * Gizmo.GetWorldAxisLength());
			REQUIRE(InPicture.has_value());
			CAPTURE(Piece);
			CHECK(Rings[0].GetDistanceTo(*InPicture) < 0.01f);
		}
		CHECK(Rings[0].Points.size() == FrontCount);

		// A ring that is seen squarely is all in front.
		for (int32 Piece = 0; Piece < FTransformGizmo::RingPieceCount; ++Piece)
		{
			CHECK(Gizmo.IsRingPointInFront(2, Piece));
		}
	}

	TEST_CASE("The part of a ring that is there is in one piece, wherever on the ring it begins")
	{
		// From straight above, with -Z up in the picture. The half of the ring around X that is towards the
		// camera begins three quarters of the way around the ring and goes on past where the ring begins.
		FCameraView View = MakeFrontView();
		View.Location = FVector(0.0, 10.0, 0.0);
		View.Rotation = glm::angleAxis(glm::radians(-90.0f), FVector3f(1.0f, 0.0f, 0.0f));
		const FTransformGizmo Gizmo(View, PictureSize, FVector(0.0), NoRotation, AxisLength);

		const std::vector<FGizmoShape> RingX = GetShapesOf(Gizmo, EGizmoMode::Rotate, EGizmoHandle::X);
		REQUIRE(RingX.size() == 1);
		const std::vector<FVector2f>& Points = RingX[0].Points;
		REQUIRE(Points.size() == static_cast<std::size_t>(FTransformGizmo::RingPieceCount) / 2 + 1);
		CHECK(IsNear(Points.front(), FVector2f(100.0f, 50.0f)));
		CHECK(IsNear(Points.back(), FVector2f(100.0f, 150.0f)));
		for (std::size_t Index = 1; Index < Points.size(); ++Index)
		{
			// From one point to the next is a piece of the ring, and never a leap across it. Where the ring is
			// nearest to the camera, it is twice the size, and so are its pieces.
			CHECK(glm::length(Points[Index] - Points[Index - 1]) < 12.0f);
			CHECK(Points[Index].x == doctest::Approx(100.0f));
		}

		// The ring around Y is seen squarely from here, and is whole.
		const std::vector<FGizmoShape> RingY = GetShapesOf(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Y);
		REQUIRE(RingY.size() == 1);
		CHECK(RingY[0].Points.size() == static_cast<std::size_t>(FTransformGizmo::RingPieceCount) + 1);
	}

	TEST_CASE("Of a ring that is seen at a slant, the part towards the camera is there, and can be taken hold of")
	{
		// As the editor's camera first looks, and from the opposite side: from below and behind.
		FViewportCamera FromBelow;
		FromBelow.Yaw = -145.0f;
		FromBelow.Pitch = 22.0f;
		for (const FViewportCamera& Camera : {FViewportCamera(), FromBelow})
		{
			const FTransformGizmo Gizmo(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), NoRotation, AxisLength);
			REQUIRE(Gizmo.IsVisible());
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				CAPTURE(AxisIndex);
				const EGizmoHandle Handle = AxisIndex == 0 ? EGizmoHandle::X
					: AxisIndex == 1                       ? EGizmoHandle::Y
														   : EGizmoHandle::Z;
				const std::vector<FGizmoShape> Ring = GetShapesOf(Gizmo, EGizmoMode::Rotate, Handle);
				REQUIRE(Ring.size() == 1);
				// More than half of the ring, and not all of it.
				CHECK(Ring[0].Points.size() > static_cast<std::size_t>(FTransformGizmo::RingPieceCount) / 2);
				CHECK(Ring[0].Points.size() < static_cast<std::size_t>(FTransformGizmo::RingPieceCount));

				// The point at which the ring is taken hold of is on the ring, in the world as in the picture,
				// and on the side of the gizmo that the camera is on.
				const std::optional<FVector2f> Point = Gizmo.FindHandlePoint(EGizmoMode::Rotate, Handle);
				REQUIRE(Point.has_value());
				CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, *Point, 0.5f) == Handle);
				const FRay Ray = Gizmo.GetRay(*Point);
				const std::optional<double> Distance = Ray.IntersectPlane(Gizmo.GetPivot(), Gizmo.GetAxis(AxisIndex));
				REQUIRE(Distance.has_value());
				const FVector FromMiddle = Ray.GetPoint(*Distance) - Gizmo.GetPivot();
				CHECK(glm::length(FromMiddle) == doctest::Approx(Gizmo.GetWorldAxisLength()).epsilon(0.01));
				CHECK(glm::dot(FromMiddle, FVector(Gizmo.GetViewDirection())) < 0.0);
			}
		}
	}

	TEST_CASE("To size things, a gizmo has a box at the end of each axis and one in the middle")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();
		const std::vector<FGizmoShape> Shapes = Gizmo.GetShapes(EGizmoMode::Scale);

		// The box in the middle comes first, so that the mouse has it where the lines of the axes begin.
		REQUIRE(Shapes.size() == 5);
		CHECK(Shapes[0].Handle == EGizmoHandle::All);
		CHECK(Shapes[0].bIsArea);
		REQUIRE(Shapes[0].Points.size() == 4);
		CHECK(IsNear(Shapes[0].Points[0], FVector2f(95.5f, 95.5f)));
		CHECK(IsNear(Shapes[0].Points[2], FVector2f(104.5f, 104.5f)));

		const std::vector<FGizmoShape> AxisX = GetShapesOf(Gizmo, EGizmoMode::Scale, EGizmoHandle::X);
		REQUIRE(AxisX.size() == 2);
		CHECK_FALSE(AxisX[0].bIsArea);
		CHECK(IsNear(AxisX[0].Points[0], FVector2f(100.0f, 100.0f)));
		CHECK(IsNear(AxisX[0].Points[1], FVector2f(150.0f, 100.0f)));
		CHECK(AxisX[1].bIsArea);
		CHECK(IsNear(AxisX[1].Points[0], FVector2f(147.0f, 97.0f)));
		CHECK(IsNear(AxisX[1].Points[2], FVector2f(153.0f, 103.0f)));
	}

	TEST_CASE("The handle at a point of the picture is the one whose shape is there, or near enough")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();
		constexpr float Reach = 5.0f;

		// On the arrows: their lines, their heads, and beside their lines within reach.
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(130.0f, 100.0f), Reach) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(146.0f, 101.0f), 0.0f) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(130.0f, 104.0f), Reach) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(130.0f, 108.0f), Reach) == EGizmoHandle::None);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(100.0f, 70.0f), Reach) == EGizmoHandle::Y);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(160.0f, 100.0f), Reach) == EGizmoHandle::None);

		// On the square, which the mouse is on wherever it is inside it.
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(118.0f, 82.0f), 0.0f) == EGizmoHandle::XY);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(10.0f, 10.0f), Reach) == EGizmoHandle::None);
	}

	TEST_CASE("The nearest of several handles is the one that the mouse has")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		// The rings: on the circle around Z, on the upright line around X and on the level one around Y.
		CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, FVector2f(135.36f, 64.64f), 3.0f) == EGizmoHandle::Z);
		CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, FVector2f(100.0f, 130.0f), 3.0f) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, FVector2f(130.0f, 100.0f), 3.0f) == EGizmoHandle::Y);
		// Between the circle and the level line, nearer to the circle; then nearer to the line.
		CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, FVector2f(141.0f, 90.0f), 20.0f) == EGizmoHandle::Z);
		CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, FVector2f(130.0f, 96.0f), 20.0f) == EGizmoHandle::Y);
		CHECK(Gizmo.FindHandle(EGizmoMode::Rotate, FVector2f(120.0f, 120.0f), 3.0f) == EGizmoHandle::None);

		// The boxes: the one in the middle where the axes begin, and those at their ends.
		CHECK(Gizmo.FindHandle(EGizmoMode::Scale, FVector2f(100.0f, 100.0f), 3.0f) == EGizmoHandle::All);
		CHECK(Gizmo.FindHandle(EGizmoMode::Scale, FVector2f(103.0f, 102.0f), 0.0f) == EGizmoHandle::All);
		CHECK(Gizmo.FindHandle(EGizmoMode::Scale, FVector2f(152.0f, 102.0f), 0.0f) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Scale, FVector2f(125.0f, 101.0f), 3.0f) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Scale, FVector2f(100.0f, 50.0f), 3.0f) == EGizmoHandle::Y);
	}

	TEST_CASE("A gizmo that is turned has its handles where its own axes are")
	{
		// A quarter turn around Z: the arrow along X points up, and the one along Y to the left.
		const FTransformGizmo Gizmo = MakeFrontGizmo(glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 0.0f, 1.0f)));

		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(100.0f, 70.0f), 3.0f) == EGizmoHandle::X);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(70.0f, 100.0f), 3.0f) == EGizmoHandle::Y);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(130.0f, 100.0f), 3.0f) == EGizmoHandle::None);
		CHECK(Gizmo.FindHandle(EGizmoMode::Translate, FVector2f(82.0f, 82.0f), 0.0f) == EGizmoHandle::XY);
	}

	TEST_CASE("A gizmo that is far beside the picture has no handle at a point of the picture")
	{
		// So far to the side and up that the corners of a box are all one number there: floats are sixteen apart
		// beyond a hundred and thirty million.
		const FTransformGizmo Gizmo(MakeFrontView(), PictureSize, FVector(2.0e6, 2.0e6, -1.0), NoRotation, AxisLength);
		REQUIRE(Gizmo.IsVisible());

		for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
		{
			CAPTURE(static_cast<int32>(Mode));
			CHECK(Gizmo.FindHandle(Mode, FVector2f(100.0f, 100.0f), 5.0f) == EGizmoHandle::None);
		}
	}

	TEST_CASE("A gizmo behind the camera, or closer than its near plane, is not there")
	{
		FCameraView View = MakeFrontView();
		View.NearPlane = 0.5f;

		for (const FVector& Pivot : {FVector(0.0, 0.0, 10.0), FVector(0.0, 0.0, -0.25), FVector(0.0, 0.0, 0.0)})
		{
			const FTransformGizmo Gizmo(View, PictureSize, Pivot, NoRotation, AxisLength);

			CHECK_FALSE(Gizmo.IsVisible());
			CHECK(Gizmo.GetWorldAxisLength() == 0.0);
			for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
			{
				CHECK(Gizmo.GetShapes(Mode).empty());
				CHECK(Gizmo.FindHandle(Mode, FVector2f(100.0f, 100.0f), 50.0f) == EGizmoHandle::None);
			}
		}
	}

	TEST_CASE("Every handle has a point at which the mouse has it")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		// Halfway along the head of an arrow, and in the middle of a square.
		const std::optional<FVector2f> ArrowX = Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::X);
		REQUIRE(ArrowX.has_value());
		CHECK(IsNear(*ArrowX, FVector2f(145.0f, 100.0f)));
		const std::optional<FVector2f> ArrowY = Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::Y);
		REQUIRE(ArrowY.has_value());
		CHECK(IsNear(*ArrowY, FVector2f(100.0f, 55.0f)));
		const std::optional<FVector2f> SquareXY = Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::XY);
		REQUIRE(SquareXY.has_value());
		CHECK(IsNear(*SquareXY, FVector2f(118.75f, 81.25f)));

		// On a ring, halfway between the two axes that it lies in. The ring around Z is seen squarely; of the two
		// that are seen from their edges, the point is on the half that is nearer to the camera, and so farther
		// from the middle than the ring's size at the gizmo's own distance would have it.
		const std::optional<FVector2f> RingZ = Gizmo.FindHandlePoint(EGizmoMode::Rotate, EGizmoHandle::Z);
		REQUIRE(RingZ.has_value());
		CHECK(IsNear(*RingZ, FVector2f(135.36f, 64.64f)));
		const std::optional<FVector2f> RingX = Gizmo.FindHandlePoint(EGizmoMode::Rotate, EGizmoHandle::X);
		REQUIRE(RingX.has_value());
		CHECK(RingX->x == doctest::Approx(100.0f));
		CHECK(RingX->y < 100.0f - 35.36f);
		const std::optional<FVector2f> RingY = Gizmo.FindHandlePoint(EGizmoMode::Rotate, EGizmoHandle::Y);
		REQUIRE(RingY.has_value());
		CHECK(RingY->x > 100.0f + 35.36f);
		CHECK(RingY->y == doctest::Approx(100.0f));

		// In the middle of a box.
		const std::optional<FVector2f> BoxX = Gizmo.FindHandlePoint(EGizmoMode::Scale, EGizmoHandle::X);
		REQUIRE(BoxX.has_value());
		CHECK(IsNear(*BoxX, FVector2f(150.0f, 100.0f)));
		const std::optional<FVector2f> BoxAll = Gizmo.FindHandlePoint(EGizmoMode::Scale, EGizmoHandle::All);
		REQUIRE(BoxAll.has_value());
		CHECK(IsNear(*BoxAll, FVector2f(100.0f, 100.0f)));
	}

	TEST_CASE("At the point of a handle the mouse has that handle, from wherever the gizmo is seen")
	{
		// From the side and above, as the editor's camera first looks: every handle is there, and none lies over
		// the point of another.
		const FViewportCamera Camera;
		for (const FQuat& Orientation : {NoRotation, glm::angleAxis(glm::radians(30.0f), FVector3f(0.0f, 1.0f, 0.0f))})
		{
			const FTransformGizmo Gizmo(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), Orientation, AxisLength);
			for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
			{
				for (const EGizmoHandle Handle : {EGizmoHandle::X,
						 EGizmoHandle::Y,
						 EGizmoHandle::Z,
						 EGizmoHandle::YZ,
						 EGizmoHandle::XZ,
						 EGizmoHandle::XY,
						 EGizmoHandle::All})
				{
					CAPTURE(static_cast<int32>(Mode));
					CAPTURE(static_cast<int32>(Handle));
					const bool bHasShape = !GetShapesOf(Gizmo, Mode, Handle).empty();
					const std::optional<FVector2f> Point = Gizmo.FindHandlePoint(Mode, Handle);

					REQUIRE(Point.has_value() == bHasShape);
					if (Point.has_value())
					{
						CHECK(Gizmo.FindHandle(Mode, *Point, 0.5f) == Handle);
					}
				}
			}
		}
	}

	TEST_CASE("A handle that a mode has not, or that cannot be used, has no point")
	{
		const FTransformGizmo Gizmo = MakeFrontGizmo();

		for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
		{
			CHECK_FALSE(Gizmo.FindHandlePoint(Mode, EGizmoHandle::None).has_value());
		}
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::All).has_value());
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Rotate, EGizmoHandle::XY).has_value());
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Rotate, EGizmoHandle::All).has_value());
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Scale, EGizmoHandle::YZ).has_value());

		// The arrow and the box along Z point straight at the camera, and the squares with Z are seen from
		// their edge.
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::Z).has_value());
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::XZ).has_value());
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Scale, EGizmoHandle::Z).has_value());

		// A gizmo behind the camera has no handles at all.
		const FTransformGizmo Behind(MakeFrontView(), PictureSize, FVector(0.0, 0.0, 10.0), NoRotation, AxisLength);
		CHECK_FALSE(Behind.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::X).has_value());
		CHECK_FALSE(Behind.FindHandlePoint(EGizmoMode::Rotate, EGizmoHandle::Z).has_value());
		CHECK_FALSE(Behind.FindHandlePoint(EGizmoMode::Scale, EGizmoHandle::All).has_value());
	}

	TEST_CASE("A handle whose point another handle lies over has no point")
	{
		// The camera is level and looks between X and -Z, so that Z leans towards it and to the right, where X
		// leans away. The squares of the planes that Y is in then lie on one another in the picture, the one
		// with Z in front, and it is the one that the mouse has where the middle of the other is.
		FCameraView View = MakeFrontView();
		View.Rotation = glm::angleAxis(glm::radians(-45.0f), FVector3f(0.0f, 1.0f, 0.0f));
		const FVector Pivot = FVector(View.Rotation * FVector3f(0.0f, 0.0f, -10.0f));
		const FTransformGizmo Gizmo(View, PictureSize, Pivot, NoRotation, AxisLength);
		REQUIRE(GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::XY).size() == 1);
		REQUIRE(GetShapesOf(Gizmo, EGizmoMode::Translate, EGizmoHandle::YZ).size() == 1);

		CHECK(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::YZ).has_value());
		CHECK_FALSE(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::XY).has_value());
		// The arrows are in the way of nothing.
		CHECK(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::X).has_value());
		CHECK(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::Y).has_value());
		CHECK(Gizmo.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::Z).has_value());
	}

	TEST_CASE("Every handle is along, around or across one axis, but for the one that is along them all")
	{
		CHECK(FTransformGizmo::GetAxisIndex(EGizmoHandle::X) == 0);
		CHECK(FTransformGizmo::GetAxisIndex(EGizmoHandle::Y) == 1);
		CHECK(FTransformGizmo::GetAxisIndex(EGizmoHandle::Z) == 2);
		// A plane is known by the axis that it leaves out.
		CHECK(FTransformGizmo::GetAxisIndex(EGizmoHandle::YZ) == 0);
		CHECK(FTransformGizmo::GetAxisIndex(EGizmoHandle::XZ) == 1);
		CHECK(FTransformGizmo::GetAxisIndex(EGizmoHandle::XY) == 2);
		CHECK_FALSE(FTransformGizmo::GetAxisIndex(EGizmoHandle::None).has_value());
		CHECK_FALSE(FTransformGizmo::GetAxisIndex(EGizmoHandle::All).has_value());

		for (const EGizmoHandle Handle : {EGizmoHandle::X, EGizmoHandle::Y, EGizmoHandle::Z})
		{
			CHECK(FTransformGizmo::IsAxisHandle(Handle));
			CHECK_FALSE(FTransformGizmo::IsPlaneHandle(Handle));
		}
		for (const EGizmoHandle Handle : {EGizmoHandle::YZ, EGizmoHandle::XZ, EGizmoHandle::XY})
		{
			CHECK(FTransformGizmo::IsPlaneHandle(Handle));
			CHECK_FALSE(FTransformGizmo::IsAxisHandle(Handle));
		}
		for (const EGizmoHandle Handle : {EGizmoHandle::None, EGizmoHandle::All})
		{
			CHECK_FALSE(FTransformGizmo::IsAxisHandle(Handle));
			CHECK_FALSE(FTransformGizmo::IsPlaneHandle(Handle));
		}
	}
}
