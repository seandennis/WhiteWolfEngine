#include "Math/Intersection.h"

#include <doctest/doctest.h>

#include <limits>
#include <optional>

namespace
{
	// A triangle in the plane z = 0 whose front faces +Z: its corners run counter-clockwise seen from there.
	const FVector3f CornerA(0.0f, 0.0f, 0.0f);
	const FVector3f CornerB(1.0f, 0.0f, 0.0f);
	const FVector3f CornerC(0.0f, 1.0f, 0.0f);

	/** Returns where a ray meets that triangle. */
	std::optional<float> MeetTriangle(const FVector3f& Origin, const FVector3f& Direction)
	{
		return FIntersection::RayTriangle(Origin, Direction, CornerA, CornerB, CornerC);
	}

	/** Returns the box from -1 to 1 along every axis. */
	FBox3f MakeCube()
	{
		FBox3f Cube;
		Cube.Add(FVector3f(-1.0f, -1.0f, -1.0f));
		Cube.Add(FVector3f(1.0f, 1.0f, 1.0f));
		return Cube;
	}

	const FVector3f Down(0.0f, 0.0f, -1.0f);
}

TEST_SUITE("FIntersection")
{
	TEST_CASE("A ray that comes at the front of a triangle meets it at its distance")
	{
		const std::optional<float> Distance = MeetTriangle(FVector3f(0.25f, 0.25f, 5.0f), Down);

		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0f));
	}

	TEST_CASE("The distance to a triangle is counted in multiples of the direction")
	{
		const FVector3f Origin(0.25f, 0.25f, 5.0f);

		const std::optional<float> Twice = MeetTriangle(Origin, FVector3f(0.0f, 0.0f, -2.0f));
		const std::optional<float> Half = MeetTriangle(Origin, FVector3f(0.0f, 0.0f, -0.5f));

		REQUIRE(Twice.has_value());
		REQUIRE(Half.has_value());
		CHECK(*Twice == doctest::Approx(2.5f));
		CHECK(*Half == doctest::Approx(10.0f));
	}

	TEST_CASE("A ray that comes at a triangle at a slant meets it where it crosses its plane")
	{
		// Three steps of (1, 1, -1) lead to the point (0.25, 0.25, 0) of the triangle.
		const FVector3f Direction(1.0f, 1.0f, -1.0f);
		const FVector3f Origin = FVector3f(0.25f, 0.25f, 0.0f) - Direction * 3.0f;

		const std::optional<float> Distance = MeetTriangle(Origin, Direction);

		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(3.0f));
	}

	TEST_CASE("A ray that passes a triangle outside one of its edges misses it")
	{
		// Left of the edge from A to C, below the edge from A to B, and beyond the edge from B to C.
		CHECK_FALSE(MeetTriangle(FVector3f(-0.1f, 0.5f, 5.0f), Down).has_value());
		CHECK_FALSE(MeetTriangle(FVector3f(0.5f, -0.1f, 5.0f), Down).has_value());
		CHECK_FALSE(MeetTriangle(FVector3f(0.6f, 0.6f, 5.0f), Down).has_value());
		CHECK_FALSE(MeetTriangle(FVector3f(20.0f, 20.0f, 5.0f), Down).has_value());
	}

	TEST_CASE("The edges and corners of a triangle belong to it")
	{
		CHECK(MeetTriangle(FVector3f(0.0f, 0.0f, 5.0f), Down).has_value());
		CHECK(MeetTriangle(FVector3f(1.0f, 0.0f, 5.0f), Down).has_value());
		CHECK(MeetTriangle(FVector3f(0.0f, 1.0f, 5.0f), Down).has_value());
		CHECK(MeetTriangle(FVector3f(0.5f, 0.0f, 5.0f), Down).has_value());
		CHECK(MeetTriangle(FVector3f(0.5f, 0.5f, 5.0f), Down).has_value());
	}

	TEST_CASE("A ray that comes at a triangle from behind meets nothing")
	{
		CHECK_FALSE(MeetTriangle(FVector3f(0.25f, 0.25f, -5.0f), FVector3f(0.0f, 0.0f, 1.0f)).has_value());

		// The same triangle with its corners the other way round has its front there.
		const std::optional<float> Distance = FIntersection::RayTriangle(
			FVector3f(0.25f, 0.25f, -5.0f), FVector3f(0.0f, 0.0f, 1.0f), CornerA, CornerC, CornerB);
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0f));
	}

	TEST_CASE("A ray along the plane of a triangle meets nothing")
	{
		// In the plane itself, through the triangle, and beside the plane.
		CHECK_FALSE(MeetTriangle(FVector3f(-5.0f, 0.25f, 0.0f), FVector3f(1.0f, 0.0f, 0.0f)).has_value());
		CHECK_FALSE(MeetTriangle(FVector3f(-5.0f, 0.25f, 1.0f), FVector3f(1.0f, 0.0f, 0.0f)).has_value());
	}

	TEST_CASE("A triangle behind where the ray begins is not met, and one that the ray begins on is")
	{
		// The ray goes the way that would meet the front, but begins beyond it.
		CHECK_FALSE(MeetTriangle(FVector3f(0.25f, 0.25f, -5.0f), Down).has_value());

		const std::optional<float> OnIt = MeetTriangle(FVector3f(0.25f, 0.25f, 0.0f), Down);
		REQUIRE(OnIt.has_value());
		CHECK(*OnIt == doctest::Approx(0.0f));
	}

	TEST_CASE("A triangle without an area is met by nothing")
	{
		const FVector3f Origin(0.25f, 0.25f, 5.0f);

		// Two corners in one place, and three corners in a line.
		CHECK_FALSE(FIntersection::RayTriangle(Origin, Down, CornerA, CornerA, CornerC).has_value());
		CHECK_FALSE(
			FIntersection::RayTriangle(Origin, Down, CornerA, CornerB, FVector3f(2.0f, 0.0f, 0.0f)).has_value());
		CHECK_FALSE(
			FIntersection::RayTriangle(FVector3f(0.0f, 0.0f, 5.0f), Down, CornerA, CornerA, CornerA).has_value());
	}

	TEST_CASE("A triangle with a corner that is no number is met by nothing")
	{
		const float NotANumber = std::numeric_limits<float>::quiet_NaN();

		CHECK_FALSE(FIntersection::RayTriangle(
			FVector3f(0.25f, 0.25f, 5.0f), Down, CornerA, FVector3f(NotANumber, 0.0f, 0.0f), CornerC)
						.has_value());
	}

	TEST_CASE("A ray through a box meets it")
	{
		const FBox3f Cube = MakeCube();

		CHECK(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 5.0f), Down, Cube));
		CHECK(FIntersection::RayBox(FVector3f(0.5f, -0.5f, 5.0f), Down, Cube));
		CHECK(FIntersection::RayBox(FVector3f(5.0f, 5.0f, 5.0f), FVector3f(-1.0f, -1.0f, -1.0f), Cube));
		// The length of the direction makes no difference.
		CHECK(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 5.0f), FVector3f(0.0f, 0.0f, -100.0f), Cube));
		CHECK(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 5.0f), FVector3f(0.0f, 0.0f, -0.001f), Cube));
	}

	TEST_CASE("A ray that passes a box, or points away from it, misses it")
	{
		const FBox3f Cube = MakeCube();

		// Beside it, along each axis in turn.
		CHECK_FALSE(FIntersection::RayBox(FVector3f(3.0f, 0.0f, 5.0f), Down, Cube));
		CHECK_FALSE(FIntersection::RayBox(FVector3f(0.0f, -3.0f, 5.0f), Down, Cube));
		CHECK_FALSE(FIntersection::RayBox(FVector3f(5.0f, 0.0f, 3.0f), FVector3f(-1.0f, 0.0f, 0.0f), Cube));
		// Towards it at first, and past a corner.
		CHECK_FALSE(FIntersection::RayBox(FVector3f(5.0f, 5.0f, 5.0f), FVector3f(-1.0f, -1.0f, -0.5f), Cube));
		// With the box behind it.
		CHECK_FALSE(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 5.0f), FVector3f(0.0f, 0.0f, 1.0f), Cube));
	}

	TEST_CASE("A ray that begins inside a box meets it, whichever way it goes")
	{
		const FBox3f Cube = MakeCube();
		const FVector3f Inside(0.5f, -0.25f, 0.75f);

		CHECK(FIntersection::RayBox(Inside, Down, Cube));
		CHECK(FIntersection::RayBox(Inside, FVector3f(0.0f, 0.0f, 1.0f), Cube));
		CHECK(FIntersection::RayBox(Inside, FVector3f(1.0f, 2.0f, -3.0f), Cube));
	}

	TEST_CASE("A ray that touches a side, an edge or a corner of a box meets it")
	{
		const FBox3f Cube = MakeCube();

		CHECK(FIntersection::RayBox(FVector3f(1.0f, 0.0f, 5.0f), Down, Cube));
		CHECK(FIntersection::RayBox(FVector3f(1.0f, 1.0f, 5.0f), Down, Cube));
		CHECK(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 1.0f), FVector3f(1.0f, 0.0f, 0.0f), Cube));
	}

	TEST_CASE("A box without thickness is a box like any other")
	{
		// The box of a floor: a square in the plane y = 0.
		FBox3f Floor;
		Floor.Add(FVector3f(-1.0f, 0.0f, -1.0f));
		Floor.Add(FVector3f(1.0f, 0.0f, 1.0f));
		const FVector3f Downwards(0.0f, -1.0f, 0.0f);
		const FVector3f Sideways(1.0f, 0.0f, 0.0f);

		CHECK(FIntersection::RayBox(FVector3f(0.5f, 5.0f, 0.5f), Downwards, Floor));
		CHECK_FALSE(FIntersection::RayBox(FVector3f(1.5f, 5.0f, 0.5f), Downwards, Floor));
		// Along the floor: in its plane it runs through the box, above it it never comes to it.
		CHECK(FIntersection::RayBox(FVector3f(-5.0f, 0.0f, 0.0f), Sideways, Floor));
		CHECK_FALSE(FIntersection::RayBox(FVector3f(-5.0f, 1.0f, 0.0f), Sideways, Floor));
	}

	TEST_CASE("An empty box is met by nothing")
	{
		const FBox3f Empty;

		// Its corners are at the origin, which is no place that it is at.
		CHECK_FALSE(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 5.0f), Down, Empty));
		CHECK_FALSE(FIntersection::RayBox(FVector3f(0.0f, 0.0f, 0.0f), Down, Empty));
	}
}
