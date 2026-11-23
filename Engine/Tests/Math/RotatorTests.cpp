#include "Math/Rotator.h"

#include "Math/Vector.h"

#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>

namespace
{
	const FVector3f Forward(0.0f, 0.0f, -1.0f);
	const FVector3f Right(1.0f, 0.0f, 0.0f);
	const FVector3f Up(0.0f, 1.0f, 0.0f);

	/** How far an angle in degrees may be from the expected one. */
	constexpr float AngleTolerance = 0.01f;

	FRotator MakeRotator(float Pitch, float Yaw, float Roll)
	{
		FRotator Rotator;
		Rotator.Pitch = Pitch;
		Rotator.Yaw = Yaw;
		Rotator.Roll = Roll;
		return Rotator;
	}

	void CheckNearlyEqual(const FVector3f& Actual, const FVector3f& Expected)
	{
		constexpr float Tolerance = 1.0e-5f;
		CHECK(std::abs(Actual.x - Expected.x) <= Tolerance);
		CHECK(std::abs(Actual.y - Expected.y) <= Tolerance);
		CHECK(std::abs(Actual.z - Expected.z) <= Tolerance);
	}

	void CheckNearlyEqual(const FRotator& Actual, const FRotator& Expected)
	{
		CHECK(std::abs(Actual.Pitch - Expected.Pitch) <= AngleTolerance);
		CHECK(std::abs(Actual.Yaw - Expected.Yaw) <= AngleTolerance);
		CHECK(std::abs(Actual.Roll - Expected.Roll) <= AngleTolerance);
	}

	/** Checks that two unit quaternions describe the same rotation; Q and -Q are the same rotation. */
	void CheckSameRotation(const FQuat& Actual, const FQuat& Expected)
	{
		constexpr float Tolerance = 1.0e-6f;
		CHECK(std::abs(glm::dot(Actual, Expected)) >= 1.0f - Tolerance);
	}
}

TEST_SUITE("FRotator")
{
	TEST_CASE("A rotator is no rotation until it is given angles")
	{
		const FRotator Rotator;

		CHECK(Rotator.Pitch == 0.0f);
		CHECK(Rotator.Yaw == 0.0f);
		CHECK(Rotator.Roll == 0.0f);
		CheckSameRotation(Rotator.ToQuat(), FQuat(1.0f, 0.0f, 0.0f, 0.0f));
	}

	TEST_CASE("A positive pitch tilts what faces forward upwards")
	{
		const FQuat Rotation = MakeRotator(90.0f, 0.0f, 0.0f).ToQuat();

		CheckNearlyEqual(Rotation * Forward, Up);
		CheckNearlyEqual(Rotation * Right, Right);
	}

	TEST_CASE("A positive yaw turns what faces forward to the left")
	{
		const FQuat Rotation = MakeRotator(0.0f, 90.0f, 0.0f).ToQuat();

		CheckNearlyEqual(Rotation * Forward, -Right);
		CheckNearlyEqual(Rotation * Up, Up);
	}

	TEST_CASE("A positive roll tips an object over onto its left side")
	{
		const FQuat Rotation = MakeRotator(0.0f, 0.0f, 90.0f).ToQuat();

		CheckNearlyEqual(Rotation * Up, -Right);
		CheckNearlyEqual(Rotation * Right, Up);
		CheckNearlyEqual(Rotation * Forward, Forward);
	}

	TEST_CASE("The yaw turns around the up axis whatever the pitch and roll are")
	{
		// Tilted up by 45 degrees and then turned to the left: forward points left and up, not along a tilted circle.
		const FQuat Rotation = MakeRotator(45.0f, 90.0f, 0.0f).ToQuat();
		const float HalfSqrtTwo = std::sqrt(0.5f);

		CheckNearlyEqual(Rotation * Forward, FVector3f(-HalfSqrtTwo, HalfSqrtTwo, 0.0f));

		// The roll turns around the way the object then faces, which it leaves alone.
		const FQuat Rolled = MakeRotator(45.0f, 90.0f, 70.0f).ToQuat();
		CheckNearlyEqual(Rolled * Forward, FVector3f(-HalfSqrtTwo, HalfSqrtTwo, 0.0f));
	}

	TEST_CASE("The angles of a rotation that was made from angles in range are those angles")
	{
		for (const FRotator& Rotator : {MakeRotator(0.0f, 0.0f, 0.0f),
				 MakeRotator(30.0f, 0.0f, 0.0f),
				 MakeRotator(0.0f, -120.0f, 0.0f),
				 MakeRotator(0.0f, 0.0f, 75.0f),
				 MakeRotator(35.0f, 140.0f, -20.0f),
				 MakeRotator(-80.0f, -179.0f, 179.0f),
				 MakeRotator(89.0f, 10.0f, 10.0f)})
		{
			CAPTURE(Rotator.Pitch);
			CAPTURE(Rotator.Yaw);
			CAPTURE(Rotator.Roll);
			CheckNearlyEqual(FRotator::FromQuat(Rotator.ToQuat()), Rotator);
		}
	}

	TEST_CASE("Angles out of range come back in range, for the same rotation")
	{
		const FRotator Rotator = MakeRotator(0.0f, 270.0f, 0.0f);

		const FRotator InRange = FRotator::FromQuat(Rotator.ToQuat());

		CheckNearlyEqual(InRange, MakeRotator(0.0f, -90.0f, 0.0f));

		// Tilted over backwards is the same as turned around and rolled over.
		const FRotator Overhead = MakeRotator(135.0f, 0.0f, 0.0f);
		const FRotator OverheadInRange = FRotator::FromQuat(Overhead.ToQuat());
		CHECK(std::abs(OverheadInRange.Pitch) <= 90.0f);
		CheckSameRotation(OverheadInRange.ToQuat(), Overhead.ToQuat());
	}

	TEST_CASE("The angles of any rotation describe that rotation")
	{
		for (const FQuat& Rotation : {FQuat(1.0f, 0.0f, 0.0f, 0.0f),
				 glm::normalize(FQuat(0.3f, -0.5f, 0.7f, 0.2f)),
				 glm::normalize(FQuat(-0.9f, 0.1f, 0.1f, 0.4f)),
				 glm::normalize(FQuat(0.0f, 1.0f, 0.0f, 0.0f)),
				 glm::normalize(FQuat(0.0f, 0.0f, 0.6f, -0.8f)),
				 glm::angleAxis(glm::radians(200.0f), glm::normalize(FVector3f(1.0f, 2.0f, 3.0f)))})
		{
			CAPTURE(Rotation.w);
			CAPTURE(Rotation.x);
			CAPTURE(Rotation.y);
			CAPTURE(Rotation.z);
			const FRotator Rotator = FRotator::FromQuat(Rotation);

			CHECK(std::abs(Rotator.Pitch) <= 90.0f);
			CHECK(std::abs(Rotator.Yaw) <= 180.0f);
			CHECK(std::abs(Rotator.Roll) <= 180.0f);
			CheckSameRotation(Rotator.ToQuat(), Rotation);
		}
	}

	TEST_CASE("Straight up and straight down, the yaw says what yaw and roll do together")
	{
		// Facing straight up, a yaw and a roll turn around the same axis, in opposite directions.
		const FRotator Upwards = MakeRotator(90.0f, 50.0f, 20.0f);
		const FRotator UpwardsRead = FRotator::FromQuat(Upwards.ToQuat());

		CheckNearlyEqual(UpwardsRead, MakeRotator(90.0f, 30.0f, 0.0f));
		CheckSameRotation(UpwardsRead.ToQuat(), Upwards.ToQuat());

		// Facing straight down, they turn in the same direction.
		const FRotator Downwards = MakeRotator(-90.0f, 50.0f, 20.0f);
		const FRotator DownwardsRead = FRotator::FromQuat(Downwards.ToQuat());

		CheckNearlyEqual(DownwardsRead, MakeRotator(-90.0f, 70.0f, 0.0f));
		CheckSameRotation(DownwardsRead.ToQuat(), Downwards.ToQuat());
	}

	TEST_CASE("Rotators with the same angles are equal")
	{
		CHECK(MakeRotator(1.0f, 2.0f, 3.0f) == MakeRotator(1.0f, 2.0f, 3.0f));
		CHECK_FALSE(MakeRotator(1.0f, 2.0f, 3.0f) == MakeRotator(1.0f, 2.0f, 4.0f));
	}
}
