#include "Math/Ray.h"

#include <doctest/doctest.h>

TEST_SUITE("FRay")
{
	TEST_CASE("A new ray begins at the origin of the world and goes the way that objects face")
	{
		const FRay Ray;

		CHECK(Ray.Origin == FVector(0.0));
		CHECK(Ray.Direction == FVector3f(0.0f, 0.0f, -1.0f));
	}

	TEST_CASE("GetPoint is the point that many metres along the ray")
	{
		FRay Ray;
		Ray.Origin = FVector(10.0, 20.0, 30.0);
		Ray.Direction = FVector3f(0.0f, 1.0f, 0.0f);

		CHECK(Ray.GetPoint(0.0) == FVector(10.0, 20.0, 30.0));
		CHECK(Ray.GetPoint(2.5) == FVector(10.0, 22.5, 30.0));
		// A distance below zero is behind where the ray begins.
		CHECK(Ray.GetPoint(-4.0) == FVector(10.0, 16.0, 30.0));
	}

	TEST_CASE("A point along a ray is exact far from the origin of the world")
	{
		// Ten million kilometres out, a step of a millimetre is still a step of a millimetre.
		FRay Ray;
		Ray.Origin = FVector(1.0e10, 0.0, 0.0);
		Ray.Direction = FVector3f(1.0f, 0.0f, 0.0f);

		CHECK(Ray.GetPoint(0.001).x - Ray.Origin.x == doctest::Approx(0.001).epsilon(1.0e-3));
	}

	TEST_CASE("Rays are equal when they begin at the same point and go the same way")
	{
		FRay Ray;
		Ray.Origin = FVector(1.0, 2.0, 3.0);
		FRay Same = Ray;
		FRay Elsewhere = Ray;
		Elsewhere.Origin.x = 1.5;
		FRay Turned = Ray;
		Turned.Direction = FVector3f(1.0f, 0.0f, 0.0f);

		CHECK(Ray == Same);
		CHECK_FALSE(Ray == Elsewhere);
		CHECK_FALSE(Ray == Turned);
	}
}
