#include "Rendering/ShadowCascades.h"

#include "Rendering/CameraView.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
	constexpr float ShadowDistance = 100.0f;
	constexpr int32 MapSize = 1024;

	const FVector3f AxisX(1.0f, 0.0f, 0.0f);
	const FVector3f AxisY(0.0f, 1.0f, 0.0f);

	FQuat Turn(float Degrees, const FVector3f& Axis)
	{
		return glm::angleAxis(glm::radians(Degrees), Axis);
	}

	/** A sun that shines down and a little sideways. */
	const FQuat SunRotation = Turn(30.0f, AxisY) * Turn(-60.0f, AxisX);

	/** Where a position, given as an offset from the camera, lands in a cascade's shadow map. */
	FVector3f ProjectRelative(const FShadowCascade& Cascade, const FVector3f& RelativePosition)
	{
		return FVector3f(Cascade.RelativeToShadow * glm::vec4(RelativePosition, 1.0f));
	}

	/** Where a position in the world lands in a cascade's shadow map, for the camera the cascade was made for. */
	FVector3f ProjectWorld(const FShadowCascade& Cascade, const FCameraView& View, const FVector& WorldPosition)
	{
		return ProjectRelative(Cascade, FVector3f(WorldPosition - View.Location));
	}

	/** Positions spread through the part of a view between two distances from the camera: its corners and inside. */
	std::vector<FVector3f> GetSlicePoints(
		const FCameraView& View, float AspectRatio, float NearDistance, float FarDistance)
	{
		const float HalfHeight = std::tan(glm::radians(View.VerticalFieldOfView) * 0.5f);
		std::vector<FVector3f> Points;
		for (const float Distance : {NearDistance, 0.5f * (NearDistance + FarDistance), FarDistance})
		{
			for (const float Across : {-1.0f, 0.0f, 1.0f})
			{
				for (const float Upwards : {-1.0f, 0.0f, 1.0f})
				{
					const FVector3f InView(
						Across * HalfHeight * AspectRatio * Distance, Upwards * HalfHeight * Distance, -Distance);
					Points.push_back(View.Rotation * InView);
				}
			}
		}
		return Points;
	}
}

TEST_SUITE("FShadowCascades")
{
	TEST_CASE("The cascades share the shadow distance, short ones near the camera and long ones far away")
	{
		const FShadowCascades Shadows =
			FShadowCascades::Compute(FCameraView(), 16.0f / 9.0f, SunRotation, ShadowDistance, MapSize);

		float PreviousFar = 0.0f;
		float PreviousLength = 0.0f;
		for (const FShadowCascade& Cascade : Shadows.Cascades)
		{
			const float Length = Cascade.FarDistance - PreviousFar;
			CHECK(Length > PreviousLength);
			PreviousFar = Cascade.FarDistance;
			PreviousLength = Length;
		}
		CHECK(Shadows.Cascades.back().FarDistance == doctest::Approx(ShadowDistance));
		// The first cascade is a small part of the whole, which is what makes near shadows sharp.
		CHECK(Shadows.Cascades.front().FarDistance < 0.05f * ShadowDistance);
	}

	TEST_CASE("Near cascades have smaller pixels than far ones")
	{
		const FShadowCascades Shadows =
			FShadowCascades::Compute(FCameraView(), 16.0f / 9.0f, SunRotation, ShadowDistance, MapSize);

		for (std::size_t Index = 1; Index < Shadows.Cascades.size(); ++Index)
		{
			CHECK(Shadows.Cascades[Index].TexelSize > Shadows.Cascades[Index - 1].TexelSize);
		}
		// With a field of view of 60 degrees the nearest cascade resolves about a centimetre.
		CHECK(Shadows.Cascades.front().TexelSize < 0.02f);
	}

	TEST_CASE("Everything the camera can see within a cascade's distances lies inside that cascade's shadow map")
	{
		for (const float FieldOfView : {30.0f, 60.0f, 110.0f})
		{
			for (const float AspectRatio : {0.5f, 1.0f, 2.4f})
			{
				for (const FQuat& CameraRotation : {FQuat(1.0f, 0.0f, 0.0f, 0.0f),
						 Turn(70.0f, AxisY),
						 Turn(-50.0f, AxisX),
						 Turn(200.0f, AxisY) * Turn(35.0f, AxisX)})
				{
					FCameraView View;
					View.Location = FVector(12.5, -3.0, 40.25);
					View.Rotation = CameraRotation;
					View.VerticalFieldOfView = FieldOfView;

					const FShadowCascades Shadows =
						FShadowCascades::Compute(View, AspectRatio, SunRotation, ShadowDistance, MapSize);

					int32 OutsideCount = 0;
					float NearDistance = 0.0f;
					for (const FShadowCascade& Cascade : Shadows.Cascades)
					{
						for (const FVector3f& Point :
							GetSlicePoints(View, AspectRatio, NearDistance, Cascade.FarDistance))
						{
							const FVector3f InMap = ProjectRelative(Cascade, Point);
							const bool bIsInside = std::abs(InMap.x) <= 1.0f && std::abs(InMap.y) <= 1.0f &&
								InMap.z >= 0.0f && InMap.z <= 1.0f;
							OutsideCount += bIsInside ? 0 : 1;
						}
						NearDistance = Cascade.FarDistance;
					}
					CAPTURE(FieldOfView);
					CAPTURE(AspectRatio);
					CHECK(OutsideCount == 0);
				}
			}
		}
	}

	TEST_CASE("Depth in a shadow map grows along the light's direction, and reaches far back towards the light")
	{
		const FCameraView View;
		const FShadowCascades Shadows = FShadowCascades::Compute(View, 1.0f, SunRotation, ShadowDistance, MapSize);
		const FVector3f LightForward = SunRotation * FVector3f(0.0f, 0.0f, -1.0f);

		for (const FShadowCascade& Cascade : Shadows.Cascades)
		{
			const FVector3f InView = View.Rotation * FVector3f(0.0f, 0.0f, -0.5f * Cascade.FarDistance);
			const FVector3f Here = ProjectRelative(Cascade, InView);
			const FVector3f TowardsLight = ProjectRelative(Cascade, InView - LightForward * 10.0f);
			const FVector3f FarTowardsLight = ProjectRelative(Cascade, InView - LightForward * (3.0f * ShadowDistance));

			// Moving along the light's direction changes only the depth, by the distance moved.
			CHECK(TowardsLight.x == doctest::Approx(Here.x).epsilon(0.001));
			CHECK(TowardsLight.y == doctest::Approx(Here.y).epsilon(0.001));
			CHECK(Here.z - TowardsLight.z == doctest::Approx(10.0f / Cascade.DepthRange).epsilon(0.001));
			// Something three shadow distances nearer the light is still in the map, so it casts its shadow.
			CHECK(FarTowardsLight.z >= 0.0f);
		}
	}

	TEST_CASE("A cascade is as large whichever way the camera looks")
	{
		FCameraView View;
		const FShadowCascades Ahead = FShadowCascades::Compute(View, 1.5f, SunRotation, ShadowDistance, MapSize);
		View.Rotation = Turn(123.0f, AxisY) * Turn(-40.0f, AxisX);
		const FShadowCascades Turned = FShadowCascades::Compute(View, 1.5f, SunRotation, ShadowDistance, MapSize);

		for (std::size_t Index = 0; Index < Ahead.Cascades.size(); ++Index)
		{
			CHECK(Turned.Cascades[Index].TexelSize == Ahead.Cascades[Index].TexelSize);
			CHECK(Turned.Cascades[Index].DepthRange == Ahead.Cascades[Index].DepthRange);
			CHECK(Turned.Cascades[Index].FarDistance == Ahead.Cascades[Index].FarDistance);
		}
	}

	TEST_CASE("When the camera moves, a shadow map moves by whole pixels, so a place in the world keeps its pixel grid")
	{
		const FVector WorldPoint(3.0, 0.5, -7.0);
		for (const double Distance : {0.0, 1.0e6})
		{
			const FVector Origin(Distance, Distance, -Distance);
			FCameraView View;
			View.Location = Origin;
			const FShadowCascades Before = FShadowCascades::Compute(View, 1.0f, SunRotation, ShadowDistance, MapSize);

			for (const FVector& Step : {FVector(0.013, 0.0, 0.0), FVector(0.4, -0.27, 0.11), FVector(-5.0, 2.0, 9.0)})
			{
				View.Location = Origin + Step;
				const FShadowCascades After =
					FShadowCascades::Compute(View, 1.0f, SunRotation, ShadowDistance, MapSize);

				for (std::size_t Index = 0; Index < Before.Cascades.size(); ++Index)
				{
					// The place's position in the map, in pixels, before and after the camera moved.
					const float PixelsPerUnit = static_cast<float>(MapSize) * 0.5f;
					FCameraView OriginView;
					OriginView.Location = Origin;
					const FVector3f First =
						ProjectWorld(Before.Cascades[Index], OriginView, Origin + WorldPoint) * PixelsPerUnit;
					const FVector3f Second =
						ProjectWorld(After.Cascades[Index], View, Origin + WorldPoint) * PixelsPerUnit;
					const float MovedX = Second.x - First.x;
					const float MovedY = Second.y - First.y;

					// It moved by a whole number of pixels, or not at all. A thousand kilometres out, single
					// precision leaves a little less of that exactness.
					const float Tolerance = Distance == 0.0 ? 0.02f : 0.1f;
					CAPTURE(Index);
					CAPTURE(Distance);
					CHECK(std::abs(MovedX - std::round(MovedX)) < Tolerance);
					CHECK(std::abs(MovedY - std::round(MovedY)) < Tolerance);
				}
			}
		}
	}

	TEST_CASE("The cascades are the same shape ten billion kilometres from the world origin")
	{
		FCameraView View;
		const FShadowCascades AtOrigin = FShadowCascades::Compute(View, 1.0f, SunRotation, ShadowDistance, MapSize);
		View.Location = FVector(1.0e13, -1.0e13, 1.0e13);
		const FShadowCascades FarAway = FShadowCascades::Compute(View, 1.0f, SunRotation, ShadowDistance, MapSize);

		for (std::size_t Index = 0; Index < AtOrigin.Cascades.size(); ++Index)
		{
			const FShadowCascade& Near = AtOrigin.Cascades[Index];
			const FShadowCascade& Far = FarAway.Cascades[Index];
			CHECK(Far.TexelSize == Near.TexelSize);
			CHECK(Far.FarDistance == Near.FarDistance);

			// A place in front of the camera lands in the same part of the map, to within the pixel the map
			// was moved by.
			const FVector3f Offset(1.0f, -0.5f, -0.4f * Near.FarDistance);
			const FVector3f Here = ProjectRelative(Near, Offset);
			const FVector3f There = ProjectRelative(Far, Offset);
			const float PixelSize = 2.0f / static_cast<float>(MapSize);
			CHECK(std::abs(There.x - Here.x) <= 1.5f * PixelSize);
			CHECK(std::abs(There.y - Here.y) <= 1.5f * PixelSize);
			CHECK(There.z == doctest::Approx(Here.z).epsilon(0.001));
		}
	}

	TEST_CASE("Impossible arguments are programming errors")
	{
		const FScopedLogCapture Capture;
		const FCameraView View;

		CHECK_THROWS_AS(
			FShadowCascades::Compute(View, 0.0f, SunRotation, ShadowDistance, MapSize), FFatalErrorException);
		CHECK_THROWS_AS(FShadowCascades::Compute(View, 1.0f, SunRotation, 0.0f, MapSize), FFatalErrorException);
		CHECK_THROWS_AS(FShadowCascades::Compute(View, 1.0f, SunRotation, ShadowDistance, 0), FFatalErrorException);
	}
}
