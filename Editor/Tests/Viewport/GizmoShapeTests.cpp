#include "Viewport/GizmoShape.h"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace
{
	FGizmoShape MakeLine(std::vector<FVector2f> Points)
	{
		FGizmoShape Shape;
		Shape.Handle = EGizmoHandle::X;
		Shape.Points = std::move(Points);
		return Shape;
	}

	FGizmoShape MakeArea(std::vector<FVector2f> Points)
	{
		FGizmoShape Shape = MakeLine(std::move(Points));
		Shape.bIsArea = true;
		return Shape;
	}
}

TEST_SUITE("FGizmoShape")
{
	TEST_CASE("A point is as far from a line as from the nearest point of it")
	{
		const FGizmoShape Line = MakeLine({FVector2f(0.0f, 0.0f), FVector2f(10.0f, 0.0f)});

		// Beside the line, on it, and beyond each of its ends.
		CHECK(Line.GetDistanceTo(FVector2f(5.0f, 3.0f)) == doctest::Approx(3.0f));
		CHECK(Line.GetDistanceTo(FVector2f(5.0f, -3.0f)) == doctest::Approx(3.0f));
		CHECK(Line.GetDistanceTo(FVector2f(5.0f, 0.0f)) == doctest::Approx(0.0f));
		CHECK(Line.GetDistanceTo(FVector2f(-4.0f, 0.0f)) == doctest::Approx(4.0f));
		CHECK(Line.GetDistanceTo(FVector2f(13.0f, 4.0f)) == doctest::Approx(5.0f));
	}

	TEST_CASE("A line that goes through several points is all of its pieces, and has two ends")
	{
		const FGizmoShape Line = MakeLine({FVector2f(0.0f, 0.0f), FVector2f(10.0f, 0.0f), FVector2f(10.0f, 10.0f)});

		CHECK(Line.GetDistanceTo(FVector2f(12.0f, 5.0f)) == doctest::Approx(2.0f));
		CHECK(Line.GetDistanceTo(FVector2f(5.0f, -1.0f)) == doctest::Approx(1.0f));
		// From its last point the line does not go back to its first: across the open corner it is far away.
		CHECK(Line.GetDistanceTo(FVector2f(0.0f, 10.0f)) == doctest::Approx(10.0f));
	}

	TEST_CASE("A point inside an area is on it, and one outside is as far as the nearest edge")
	{
		FGizmoShape Triangle = MakeArea({FVector2f(0.0f, 0.0f), FVector2f(10.0f, 0.0f), FVector2f(0.0f, 10.0f)});

		SUBCASE("With its corners one way round")
		{
		}
		SUBCASE("With its corners the other way round")
		{
			Triangle = MakeArea({FVector2f(0.0f, 10.0f), FVector2f(10.0f, 0.0f), FVector2f(0.0f, 0.0f)});
		}

		CHECK(Triangle.GetDistanceTo(FVector2f(2.0f, 2.0f)) == 0.0f);
		CHECK(Triangle.GetDistanceTo(FVector2f(4.0f, 5.0f)) == 0.0f);
		// On an edge and on a corner.
		CHECK(Triangle.GetDistanceTo(FVector2f(5.0f, 0.0f)) == 0.0f);
		CHECK(Triangle.GetDistanceTo(FVector2f(0.0f, 10.0f)) == 0.0f);
		// Beyond the long edge, beside a short one, and out from a corner.
		CHECK(Triangle.GetDistanceTo(FVector2f(10.0f, 10.0f)) == doctest::Approx(std::sqrt(50.0f)));
		CHECK(Triangle.GetDistanceTo(FVector2f(5.0f, -2.0f)) == doctest::Approx(2.0f));
		CHECK(Triangle.GetDistanceTo(FVector2f(-3.0f, -4.0f)) == doctest::Approx(5.0f));
	}

	TEST_CASE("An area with four corners is the whole of what they are around")
	{
		const FGizmoShape Square = MakeArea(
			{FVector2f(10.0f, 10.0f), FVector2f(20.0f, 10.0f), FVector2f(20.0f, 20.0f), FVector2f(10.0f, 20.0f)});

		CHECK(Square.GetDistanceTo(FVector2f(15.0f, 15.0f)) == 0.0f);
		CHECK(Square.GetDistanceTo(FVector2f(19.5f, 10.5f)) == 0.0f);
		CHECK(Square.GetDistanceTo(FVector2f(25.0f, 15.0f)) == doctest::Approx(5.0f));
		CHECK(Square.GetDistanceTo(FVector2f(15.0f, 4.0f)) == doctest::Approx(6.0f));
	}

	TEST_CASE("A shape of one point is that point, and one of none is nowhere")
	{
		CHECK(MakeLine({FVector2f(3.0f, 4.0f)}).GetDistanceTo(FVector2f(0.0f, 0.0f)) == doctest::Approx(5.0f));
		CHECK(MakeArea({FVector2f(3.0f, 4.0f)}).GetDistanceTo(FVector2f(0.0f, 0.0f)) == doctest::Approx(5.0f));
		CHECK(MakeLine({}).GetDistanceTo(FVector2f(0.0f, 0.0f)) == std::numeric_limits<float>::infinity());
		CHECK(MakeArea({}).GetDistanceTo(FVector2f(0.0f, 0.0f)) == std::numeric_limits<float>::infinity());
	}

	TEST_CASE("Two points in one place are a piece of no length, and no harm")
	{
		const FGizmoShape Line = MakeLine({FVector2f(1.0f, 1.0f), FVector2f(1.0f, 1.0f), FVector2f(5.0f, 1.0f)});

		CHECK(Line.GetDistanceTo(FVector2f(1.0f, 4.0f)) == doctest::Approx(3.0f));
		CHECK(Line.GetDistanceTo(FVector2f(3.0f, 2.0f)) == doctest::Approx(1.0f));
	}
}
