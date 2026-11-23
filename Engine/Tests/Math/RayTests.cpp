#include "Math/Ray.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <limits>
#include <optional>

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

	TEST_CASE("A ray goes through a plane at the distance of the plane along it, from either side")
	{
		const FVector3f Up(0.0f, 1.0f, 0.0f);
		FRay Ray;
		Ray.Origin = FVector(2.0, 5.0, -3.0);
		Ray.Direction = FVector3f(0.0f, -1.0f, 0.0f);

		// Straight down onto a floor, and onto one that is not where the world begins.
		std::optional<double> Distance = Ray.IntersectPlane(FVector(0.0), Up);
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0));
		Distance = Ray.IntersectPlane(FVector(100.0, 1.5, 100.0), Up);
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(3.5));

		// A floor that faces down is gone through as well.
		Distance = Ray.IntersectPlane(FVector(0.0), -Up);
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0));

		// At a slant the way is longer: at 45 degrees, by the square root of two.
		Ray.Direction = glm::normalize(FVector3f(1.0f, -1.0f, 0.0f));
		Distance = Ray.IntersectPlane(FVector(0.0), Up);
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0 * std::sqrt(2.0)));
		CHECK(Ray.GetPoint(*Distance).x == doctest::Approx(7.0));
		CHECK(Ray.GetPoint(*Distance).y == doctest::Approx(0.0).epsilon(1.0e-6));
	}

	TEST_CASE("A ray that runs along a plane, or away from it, does not go through it")
	{
		const FVector3f Up(0.0f, 1.0f, 0.0f);
		FRay Ray;
		Ray.Origin = FVector(0.0, 5.0, 0.0);

		Ray.Direction = FVector3f(1.0f, 0.0f, 0.0f);
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0), Up).has_value());

		Ray.Direction = FVector3f(0.0f, 1.0f, 0.0f);
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0), Up).has_value());

		// One that begins on the plane is through it at once.
		Ray.Origin = FVector(3.0, 0.0, 3.0);
		Ray.Direction = FVector3f(0.0f, -1.0f, 0.0f);
		const std::optional<double> Distance = Ray.IntersectPlane(FVector(0.0), Up);
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(0.0));
	}

	TEST_CASE("A ray or a plane that is not made of finite numbers is gone through nowhere")
	{
		const double NoNumber = std::numeric_limits<double>::quiet_NaN();
		const double Endless = std::numeric_limits<double>::infinity();
		const float NoDirection = std::numeric_limits<float>::quiet_NaN();
		const FVector3f Up(0.0f, 1.0f, 0.0f);
		FRay Ray;
		Ray.Origin = FVector(0.0, 5.0, 0.0);
		Ray.Direction = FVector3f(0.0f, -1.0f, 0.0f);
		REQUIRE(Ray.IntersectPlane(FVector(0.0), Up).has_value());

		// A plane that is nowhere, one that is endlessly far away, and one that faces no way.
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0, NoNumber, 0.0), Up).has_value());
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0, -Endless, 0.0), Up).has_value());
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0), FVector3f(0.0f, NoDirection, 0.0f)).has_value());

		// A ray that goes no way, and one that begins nowhere.
		Ray.Direction = FVector3f(NoDirection, -1.0f, 0.0f);
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0), Up).has_value());
		Ray.Direction = FVector3f(0.0f, -1.0f, 0.0f);
		Ray.Origin = FVector(NoNumber, 5.0, 0.0);
		CHECK_FALSE(Ray.IntersectPlane(FVector(0.0), Up).has_value());
	}

	TEST_CASE("Where a ray goes through a plane is exact far from the origin of the world")
	{
		FRay Ray;
		Ray.Origin = FVector(1.0e10, 2.25, -1.0e10);
		Ray.Direction = FVector3f(0.0f, -1.0f, 0.0f);

		const std::optional<double> Distance =
			Ray.IntersectPlane(FVector(1.0e10, 0.0, 1.0e10), FVector3f(0.0f, 1.0f, 0.0f));

		REQUIRE(Distance.has_value());
		CHECK(*Distance == 2.25);
	}
}
