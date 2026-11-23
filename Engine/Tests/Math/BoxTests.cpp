#include "Math/Box.h"

#include <doctest/doctest.h>

#include <cmath>

TEST_SUITE("TBox")
{
	TEST_CASE("A new box is empty and has no size")
	{
		const FBox Box;

		CHECK(Box.bIsEmpty);
		CHECK(Box.GetCenter() == FVector(0.0));
		CHECK(Box.GetSize() == FVector(0.0));
		CHECK(Box.GetRadius() == 0.0);
	}

	TEST_CASE("A box with one point is that point")
	{
		FBox Box;

		Box.Add(FVector(1.0, -2.0, 3.0));

		CHECK_FALSE(Box.bIsEmpty);
		CHECK(Box.Min == FVector(1.0, -2.0, 3.0));
		CHECK(Box.Max == FVector(1.0, -2.0, 3.0));
		CHECK(Box.GetCenter() == FVector(1.0, -2.0, 3.0));
		CHECK(Box.GetRadius() == 0.0);
	}

	TEST_CASE("A box grows to contain every point that is added, whatever the order")
	{
		FBox Box;

		Box.Add(FVector(1.0, 5.0, -1.0));
		Box.Add(FVector(-3.0, 2.0, 4.0));
		Box.Add(FVector(0.0, 9.0, 0.0));
		// A point inside changes nothing.
		Box.Add(FVector(0.0, 3.0, 0.0));

		CHECK(Box.Min == FVector(-3.0, 2.0, -1.0));
		CHECK(Box.Max == FVector(1.0, 9.0, 4.0));
		CHECK(Box.GetCenter() == FVector(-1.0, 5.5, 1.5));
		CHECK(Box.GetSize() == FVector(4.0, 7.0, 5.0));
		CHECK(Box.GetRadius() == doctest::Approx(0.5 * std::sqrt(16.0 + 49.0 + 25.0)));
	}

	TEST_CASE("Adding a box makes a box that contains both, and an empty box adds nothing")
	{
		FBox Box;
		Box.Add(FVector(0.0));
		Box.Add(FVector(1.0));
		FBox Other;
		Other.Add(FVector(-5.0, 0.5, 0.5));
		Other.Add(FVector(-4.0, 2.0, 0.5));

		Box.Add(Other);

		CHECK(Box.Min == FVector(-5.0, 0.0, 0.0));
		CHECK(Box.Max == FVector(1.0, 2.0, 1.0));

		const FBox Before = Box;
		Box.Add(FBox());
		CHECK(Box == Before);

		// An empty box that is given a box becomes that box.
		FBox Empty;
		Empty.Add(Other);
		CHECK(Empty == Other);
	}

	TEST_CASE("A box in single precision works the same way")
	{
		FBox3f Box;

		Box.Add(FVector3f(-1.0f, -2.0f, -3.0f));
		Box.Add(FVector3f(1.0f, 2.0f, 3.0f));

		CHECK(Box.GetCenter() == FVector3f(0.0f));
		CHECK(Box.GetSize() == FVector3f(2.0f, 4.0f, 6.0f));
		CHECK(Box.GetRadius() == doctest::Approx(std::sqrt(14.0f)));
	}

	TEST_CASE("Boxes are equal if they hold the same room")
	{
		FBox First;
		FBox Second;
		CHECK(First == Second);

		First.Add(FVector(1.0));
		CHECK_FALSE(First == Second);

		Second.Add(FVector(1.0));
		CHECK(First == Second);
	}
}
