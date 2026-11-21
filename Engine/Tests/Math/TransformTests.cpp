#include "Math/Transform.h"

#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>

namespace
{
	/** Returns a rotation of AngleInDegrees around Axis, which need not be normalized. */
	FQuat MakeRotation(float AngleInDegrees, const FVector3f& Axis)
	{
		return glm::angleAxis(glm::radians(AngleInDegrees), glm::normalize(Axis));
	}

	void CheckNearlyEqual(const FVector& Actual, const FVector& Expected, double Tolerance)
	{
		CHECK(std::abs(Actual.x - Expected.x) <= Tolerance);
		CHECK(std::abs(Actual.y - Expected.y) <= Tolerance);
		CHECK(std::abs(Actual.z - Expected.z) <= Tolerance);
	}

	void CheckNearlyEqual(const FVector3f& Actual, const FVector3f& Expected, float Tolerance)
	{
		CHECK(std::abs(Actual.x - Expected.x) <= Tolerance);
		CHECK(std::abs(Actual.y - Expected.y) <= Tolerance);
		CHECK(std::abs(Actual.z - Expected.z) <= Tolerance);
	}

	/** Checks that two unit quaternions describe the same rotation; Q and -Q are the same rotation. */
	void CheckSameRotation(const FQuat& Actual, const FQuat& Expected, float Tolerance)
	{
		CHECK(std::abs(glm::dot(Actual, Expected)) >= 1.0f - Tolerance);
	}

	/** An arbitrary transform with translation, rotation and uniform scale. */
	FTransform MakeUniformTransform()
	{
		FTransform Transform;
		Transform.Location = FVector(10.0, -20.0, 30.0);
		Transform.Rotation = MakeRotation(35.0f, FVector3f(1.0f, 2.0f, 3.0f));
		Transform.Scale = FVector3f(2.0f);
		return Transform;
	}

	/** An arbitrary transform with translation, rotation and non-uniform scale. */
	FTransform MakeNonUniformTransform()
	{
		FTransform Transform;
		Transform.Location = FVector(-4.0, 5.0, 6.0);
		Transform.Rotation = MakeRotation(-110.0f, FVector3f(0.0f, 1.0f, 1.0f));
		Transform.Scale = FVector3f(0.5f, 2.0f, 3.0f);
		return Transform;
	}

	// Rotations are single precision, so positions that pass through one carry a relative error near 1e-7.
	constexpr double RotatedPositionTolerance = 1.0e-4;
	constexpr float RotationTolerance = 1.0e-6f;
	constexpr float ScaleTolerance = 1.0e-6f;
}

TEST_SUITE("FTransform")
{
	TEST_CASE("A default-constructed transform is the identity")
	{
		const FTransform Transform;

		CHECK(Transform.Location == FVector(0.0, 0.0, 0.0));
		CHECK(Transform.Rotation == FQuat(1.0f, 0.0f, 0.0f, 0.0f));
		CHECK(Transform.Scale == FVector3f(1.0f, 1.0f, 1.0f));

		const FVector Position(1.5, -2.5, 3.5);
		CHECK(Transform.TransformPosition(Position) == Position);
		CHECK(Transform.InverseTransformPosition(Position) == Position);
	}

	TEST_CASE("TransformPosition translates")
	{
		FTransform Transform;
		Transform.Location = FVector(10.0, 20.0, 30.0);

		CHECK(Transform.TransformPosition(FVector(1.0, 2.0, 3.0)) == FVector(11.0, 22.0, 33.0));
	}

	TEST_CASE("TransformPosition scales each axis")
	{
		FTransform Transform;
		Transform.Scale = FVector3f(2.0f, 3.0f, 4.0f);

		CHECK(Transform.TransformPosition(FVector(1.0, 1.0, 1.0)) == FVector(2.0, 3.0, 4.0));
	}

	TEST_CASE("TransformPosition rotates counter-clockwise about the axis for a positive angle")
	{
		FTransform Transform;
		Transform.Rotation = MakeRotation(90.0f, FVector3f(0.0f, 0.0f, 1.0f));

		CheckNearlyEqual(
			Transform.TransformPosition(FVector(1.0, 0.0, 0.0)), FVector(0.0, 1.0, 0.0), RotatedPositionTolerance);
	}

	TEST_CASE("TransformPosition scales, then rotates, then translates")
	{
		FTransform Transform;
		Transform.Location = FVector(10.0, 0.0, 0.0);
		Transform.Rotation = MakeRotation(90.0f, FVector3f(0.0f, 0.0f, 1.0f));
		Transform.Scale = FVector3f(2.0f, 1.0f, 1.0f);

		// (1, 0, 0) is scaled to (2, 0, 0), rotated to (0, 2, 0) and translated to (10, 2, 0).
		CheckNearlyEqual(
			Transform.TransformPosition(FVector(1.0, 0.0, 0.0)), FVector(10.0, 2.0, 0.0), RotatedPositionTolerance);
	}

	TEST_CASE("InverseTransformPosition undoes TransformPosition")
	{
		const FVector Positions[] = {
			FVector(0.0, 0.0, 0.0),
			FVector(1.0, 2.0, 3.0),
			FVector(-7.5, 0.25, 100.0),
		};

		for (const FTransform& Transform : {MakeUniformTransform(), MakeNonUniformTransform()})
		{
			for (const FVector& Position : Positions)
			{
				const FVector RoundTripped = Transform.InverseTransformPosition(Transform.TransformPosition(Position));

				CheckNearlyEqual(RoundTripped, Position, RotatedPositionTolerance);
			}
		}
	}

	TEST_CASE("InverseTransformPosition yields zero along an axis whose scale is zero")
	{
		FTransform Transform;
		Transform.Location = FVector(1.0, 2.0, 3.0);
		Transform.Scale = FVector3f(0.0f, 2.0f, 4.0f);

		const FVector Local = Transform.InverseTransformPosition(FVector(5.0, 6.0, 7.0));

		CHECK(Local == FVector(0.0, 2.0, 1.0));
	}

	TEST_CASE("Combine applies the child and then the parent")
	{
		const FTransform Parent = MakeUniformTransform();
		const FTransform Child = MakeNonUniformTransform();
		const FTransform Combined = FTransform::Combine(Parent, Child);

		const FVector Positions[] = {
			FVector(0.0, 0.0, 0.0),
			FVector(1.0, 2.0, 3.0),
			FVector(-7.5, 0.25, 100.0),
		};
		for (const FVector& Position : Positions)
		{
			const FVector Expected = Parent.TransformPosition(Child.TransformPosition(Position));

			CheckNearlyEqual(Combined.TransformPosition(Position), Expected, 1.0e-3);
		}
	}

	TEST_CASE("Combine with the identity changes nothing")
	{
		const FTransform Identity;
		const FTransform Transform = MakeNonUniformTransform();

		const FTransform AsChild = FTransform::Combine(Identity, Transform);
		const FTransform AsParent = FTransform::Combine(Transform, Identity);

		CheckNearlyEqual(AsChild.Location, Transform.Location, 1.0e-12);
		CheckSameRotation(AsChild.Rotation, Transform.Rotation, RotationTolerance);
		CheckNearlyEqual(AsChild.Scale, Transform.Scale, ScaleTolerance);

		CheckNearlyEqual(AsParent.Location, Transform.Location, 1.0e-12);
		CheckSameRotation(AsParent.Rotation, Transform.Rotation, RotationTolerance);
		CheckNearlyEqual(AsParent.Scale, Transform.Scale, ScaleTolerance);
	}

	TEST_CASE("Combine multiplies scales and composes rotations")
	{
		FTransform Parent;
		Parent.Rotation = MakeRotation(90.0f, FVector3f(0.0f, 0.0f, 1.0f));
		Parent.Scale = FVector3f(2.0f);
		FTransform Child;
		Child.Rotation = MakeRotation(90.0f, FVector3f(0.0f, 0.0f, 1.0f));
		Child.Scale = FVector3f(1.0f, 3.0f, 5.0f);

		const FTransform Combined = FTransform::Combine(Parent, Child);

		CheckNearlyEqual(Combined.Scale, FVector3f(2.0f, 6.0f, 10.0f), ScaleTolerance);
		CheckSameRotation(Combined.Rotation, MakeRotation(180.0f, FVector3f(0.0f, 0.0f, 1.0f)), RotationTolerance);
	}

	TEST_CASE("The rotation stays normalized through a long chain of combinations")
	{
		FTransform Step;
		Step.Rotation = MakeRotation(7.0f, FVector3f(1.0f, 1.0f, 0.0f));

		FTransform Accumulated;
		for (int32 Index = 0; Index < 10000; ++Index)
		{
			Accumulated = FTransform::Combine(Accumulated, Step);
		}

		CHECK(std::abs(glm::length(Accumulated.Rotation) - 1.0f) <= 1.0e-5f);
	}

	TEST_CASE("GetRelativeTransform is the inverse of Combine")
	{
		const FTransform Parent = MakeUniformTransform();
		const FTransform World = MakeNonUniformTransform();

		const FTransform Relative = World.GetRelativeTransform(Parent);
		const FTransform Recombined = FTransform::Combine(Parent, Relative);

		CheckNearlyEqual(Recombined.Location, World.Location, RotatedPositionTolerance);
		CheckSameRotation(Recombined.Rotation, World.Rotation, RotationTolerance);
		CheckNearlyEqual(Recombined.Scale, World.Scale, 1.0e-5f);
	}

	TEST_CASE("A transform relative to itself is the identity")
	{
		const FTransform Transform = MakeUniformTransform();

		const FTransform Relative = Transform.GetRelativeTransform(Transform);

		CheckNearlyEqual(Relative.Location, FVector(0.0, 0.0, 0.0), RotatedPositionTolerance);
		CheckSameRotation(Relative.Rotation, FQuat(1.0f, 0.0f, 0.0f, 0.0f), RotationTolerance);
		CheckNearlyEqual(Relative.Scale, FVector3f(1.0f, 1.0f, 1.0f), ScaleTolerance);
	}

	TEST_CASE("GetRelativeTransform yields zero along a parent axis whose scale is zero")
	{
		FTransform Parent;
		Parent.Scale = FVector3f(0.0f, 1.0f, 2.0f);
		FTransform World;
		World.Location = FVector(4.0, 6.0, 8.0);
		World.Scale = FVector3f(3.0f, 3.0f, 3.0f);

		const FTransform Relative = World.GetRelativeTransform(Parent);

		CHECK(Relative.Location == FVector(0.0, 6.0, 4.0));
		CHECK(Relative.Scale == FVector3f(0.0f, 3.0f, 1.5f));
	}

	TEST_CASE("A small offset survives 1,000 km from the origin")
	{
		// At this distance a single-precision position could only resolve steps of about 6 cm.
		FTransform Parent;
		Parent.Location = FVector(1.0e6, -1.0e6, 1.0e6);
		FTransform Child;
		Child.Location = FVector(0.001, 0.002, -0.003);

		const FTransform World = FTransform::Combine(Parent, Child);

		CheckNearlyEqual(World.Location, FVector(1000000.001, -999999.998, 999999.997), 1.0e-9);
		CheckNearlyEqual(World.GetRelativeTransform(Parent).Location, Child.Location, 1.0e-9);
	}

	TEST_CASE("A rotated offset survives 10 billion km from the origin")
	{
		// Doubles resolve about 2 mm at 1e13 m, so a one-metre offset must come through to within a centimetre.
		FTransform Parent;
		Parent.Location = FVector(1.0e13, 0.0, 0.0);
		Parent.Rotation = MakeRotation(90.0f, FVector3f(0.0f, 0.0f, 1.0f));
		FTransform Child;
		Child.Location = FVector(1.0, 0.0, 0.0);

		const FTransform World = FTransform::Combine(Parent, Child);

		CheckNearlyEqual(World.Location, FVector(1.0e13, 1.0, 0.0), 1.0e-2);
		CheckNearlyEqual(World.GetRelativeTransform(Parent).Location, Child.Location, 1.0e-2);
	}

	TEST_CASE("Transforms compare equal only when every component matches")
	{
		const FTransform Transform = MakeNonUniformTransform();
		FTransform Other = Transform;
		CHECK(Transform == Other);

		Other.Location.x += 1.0;
		CHECK(Transform != Other);

		Other = Transform;
		Other.Scale.z += 1.0f;
		CHECK(Transform != Other);

		Other = Transform;
		Other.Rotation = MakeRotation(1.0f, FVector3f(1.0f, 0.0f, 0.0f));
		CHECK(Transform != Other);
	}
}
