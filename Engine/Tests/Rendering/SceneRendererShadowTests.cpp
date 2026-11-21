#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/HdrImage.h"
#include "Rendering/ImageDifference.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SkyLightComponent.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneLighting.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

// The shadow tests use one arrangement, at several sizes. The camera looks along -Z at a wall that faces it, and
// between the two, in the middle of the picture, floats a small square, the occluder. A sun shines from behind the
// camera, turned 45 degrees to one side, so the occluder's shadow falls on the wall beside the occluder as the
// camera sees it: two metres to the side for every two metres between occluder and wall.
//
// With the wide test camera and a picture of 65 pixels, the wall four units away shows 8.125 pixels per unit.
// The occluder covers the middle of the picture, pixels 25 to 40. Its shadow, one unit wide and high and two
// units to the left, covers pixels 13 to 19 across and 29 to 35 down.

namespace
{
	/** Where the middle of the occluder's shadow is in the picture when the sun is turned to the right. */
	constexpr int32 ShadowColumn = 16;

	/** The same distance to the other side of the middle, where the wall is lit. */
	constexpr int32 LitColumn = 48;

	/** A sun turned 45 degrees about the vertical axis: it shines towards -X and -Z. */
	const FQuat SunFromTheRight = Turn(45.0f, AxisY);

	/** The same from the other side: the shadow then falls to the right of the occluder. */
	const FQuat SunFromTheLeft = Turn(-45.0f, AxisY);

	FVector GetDirectionToSun(const FQuat& Rotation)
	{
		return -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
	}

	/** The arrangement: a wall and an occluder, both matte grey, Scale times the size described above. */
	struct FShadowScene
	{
		FShadowScene(WScene& Scene, double Scale, const FVector& Origin = FVector(0.0))
		{
			const FVector3f WallSize(static_cast<float>(6.0 * Scale));
			const FVector3f OccluderSize(static_cast<float>(Scale));
			Wall = AddSurface(Scene,
				Surface,
				MakeTransform(Origin + FVector(0.0, 0.0, -4.0 * Scale), FQuat(1.0f, 0.0f, 0.0f, 0.0f), WallSize));
			Occluder = AddSurface(Scene,
				Surface,
				MakeTransform(Origin + FVector(0.0, 0.0, -2.0 * Scale), FQuat(1.0f, 0.0f, 0.0f, 0.0f), OccluderSize));
		}

		const FReferenceSurface Surface;
		WStaticMeshComponent* Wall = nullptr;
		WStaticMeshComponent* Occluder = nullptr;
	};

	/**
	 * What the lighting model gives for a pixel of the wall, which faces the camera, lit by a sun and nothing else.
	 *
	 * @param WallDistance How far in front of the camera the wall is.
	 */
	FColor GetLitWallPixel(
		int32 X, int32 Y, const FQuat& SunRotation, double WallDistance = 4.0, double Illuminance = std::numbers::pi)
	{
		// With a field of view of 90 degrees, the edge of the picture is as far to the side as the wall is away.
		const double Across = (static_cast<double>(X) + 0.5) / (0.5 * PictureSize) - 1.0;
		const double Upwards = 1.0 - (static_cast<double>(Y) + 0.5) / (0.5 * PictureSize);
		const FVector Point(Across * WallDistance, Upwards * WallDistance, -WallDistance);

		FReferenceSurface Surface;
		Surface.ToCamera = glm::normalize(-Point);
		return ReferencePixel(ReferenceReflect(Surface, GetDirectionToSun(SunRotation), ReferenceWhite * Illuminance));
	}
}

TEST_SUITE("FSceneRenderer shadows")
{
	TEST_CASE("What stands in a sun's light casts a shadow on what is behind it")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight);

		const FImage Image = RenderPicture(Test);

		// In the shadow no light arrives at all; the same place on the other side is lit as if nothing were there.
		CHECK(Image.GetPixel(ShadowColumn, Middle) == Black);
		CHECK(Image.GetPixel(ShadowColumn - 1, Middle - 1) == Black);
		CHECK(Image.GetPixel(ShadowColumn + 1, Middle + 1) == Black);
		CheckPixel(Image, LitColumn, Middle, GetLitWallPixel(LitColumn, Middle, SunFromTheRight));
		// Above and below the shadow, and between it and the occluder, the wall is lit too.
		CheckPixel(Image, ShadowColumn, Middle - 8, GetLitWallPixel(ShadowColumn, Middle - 8, SunFromTheRight));
		CheckPixel(Image, ShadowColumn, Middle + 8, GetLitWallPixel(ShadowColumn, Middle + 8, SunFromTheRight));
		CheckPixel(Image, ShadowColumn + 6, Middle, GetLitWallPixel(ShadowColumn + 6, Middle, SunFromTheRight));
		CheckPixel(Image, ShadowColumn - 8, Middle, GetLitWallPixel(ShadowColumn - 8, Middle, SunFromTheRight));
		// The occluder itself is in the light.
		CHECK(Image.GetPixel(Middle, Middle) != Black);
		CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 4);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A shadow falls away from the sun")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		WDirectionalLightComponent* const Sun = AddSun(*Test.Scene.Get(), SunFromTheLeft);

		const FImage FromTheLeft = RenderPicture(Test);
		CHECK(FromTheLeft.GetPixel(LitColumn, Middle) == Black);
		CheckPixel(FromTheLeft, ShadowColumn, Middle, GetLitWallPixel(ShadowColumn, Middle, SunFromTheLeft));

		// From above and behind the camera, the shadow falls below the occluder.
		const FQuat SunFromAbove = Turn(-45.0f, AxisX);
		Sun->GetOwner()->SetRelativeTransform(MakeTransform(FVector(0.0), SunFromAbove));
		const FImage FromAbove = RenderPicture(Test);
		CHECK(FromAbove.GetPixel(Middle, LitColumn) == Black);
		CheckPixel(FromAbove, Middle, ShadowColumn, GetLitWallPixel(Middle, ShadowColumn, SunFromAbove));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The edge of a shadow is soft")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight);
		FCameraView View = MakeWideView();

		// A large picture, so that the edge covers several pixels: it must pass from lit to dark through shades
		// in between, without going back and forth.
		const int32 Size = 520;
		const FImage Image = Test.Render(View, Size, Size);
		const int32 Row = Size / 2;
		const int32 Lit = Image.GetPixel(Size * 3 / 4, Row).R;

		int32 ShadeCount = 0;
		int32 Reversals = 0;
		int32 Previous = Image.GetPixel(Size * 13 / 65 + 4, Row).R;
		// From inside the shadow leftwards into the light.
		for (int32 X = Size * 13 / 65 + 4; X > Size * 13 / 65 - 14; --X)
		{
			const int32 Value = Image.GetPixel(X, Row).R;
			ShadeCount += Value > 8 && Value < Lit - 8 ? 1 : 0;
			Reversals += Value + 2 < Previous ? 1 : 0;
			Previous = Value;
		}
		CHECK(Image.GetPixel(Size * 16 / 65, Row) == Black);
		CHECK(ShadeCount >= 2);
		CHECK(Reversals == 0);
	}

	TEST_CASE("A surface does not shadow itself, however flat the sun shines on it")
	{
		for (const float Degrees : {0.0f, 20.0f, 45.0f, 60.0f, 75.0f, 85.0f})
		{
			for (const FVector3f& Axis : {AxisY, AxisX})
			{
				FSceneRenderTest Test(ESceneViewMode::Lit);
				const FReferenceSurface Surface;
				AddSurface(*Test.Scene.Get(),
					Surface,
					MakeTransform(FVector(0.0, 0.0, -4.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(6.0f)));
				const FQuat Rotation = Turn(Degrees, Axis);
				AddSun(*Test.Scene.Get(), Rotation);

				const FImage Image = RenderPicture(Test);

				// Every part of the wall is lit exactly as the model says; a wall that shadowed itself would
				// show dark speckles or bands.
				int32 WrongPixelCount = 0;
				for (int32 Y = 10; Y < PictureSize - 10; Y += 3)
				{
					for (int32 X = 10; X < PictureSize - 10; X += 3)
					{
						WrongPixelCount += IsClose(Image.GetPixel(X, Y), GetLitWallPixel(X, Y, Rotation)) ? 0 : 1;
					}
				}
				CAPTURE(Degrees);
				CHECK(WrongPixelCount == 0);
				CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 4);
			}
		}
	}

	TEST_CASE("Shadows are drawn near the camera and far from it, up to the shadow distance")
	{
		// The same arrangement at growing sizes, which puts the wall in each of the four cascades in turn: with a
		// shadow distance of 100 metres they end at about 3, 18, 49 and 100 metres.
		for (const double Scale : {0.5, 1.0, 3.0, 8.0, 15.0, 22.0})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			const FShadowScene Scene(*Test.Scene.Get(), Scale);
			AddSun(*Test.Scene.Get(), SunFromTheRight);

			const FImage Image = RenderPicture(Test);

			CAPTURE(Scale);
			CHECK(Image.GetPixel(ShadowColumn, Middle) == Black);
			CheckPixel(Image, LitColumn, Middle, GetLitWallPixel(LitColumn, Middle, SunFromTheRight, 4.0 * Scale));
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("Beyond the shadow distance there are no shadows, and the distance can be changed")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		// The wall is 160 metres away.
		const FShadowScene Scene(*Test.Scene.Get(), 40.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight);

		const FColor Lit = GetLitWallPixel(ShadowColumn, Middle, SunFromTheRight, 160.0);
		CheckPixel(RenderPicture(Test), ShadowColumn, Middle, Lit);

		Test.Renderer->ShadowDistance = 400.0f;
		CHECK(RenderPicture(Test).GetPixel(ShadowColumn, Middle) == Black);

		// A distance of zero switches shadows off altogether.
		Test.Renderer->ShadowDistance = 0.0f;
		CheckPixel(RenderPicture(Test), ShadowColumn, Middle, Lit);
		CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Lights and meshes cast shadows only if they are set to")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		WDirectionalLightComponent* const Sun = AddSun(*Test.Scene.Get(), SunFromTheRight);
		const FColor Lit = GetLitWallPixel(ShadowColumn, Middle, SunFromTheRight);
		REQUIRE(RenderPicture(Test).GetPixel(ShadowColumn, Middle) == Black);

		Sun->bCastShadows = false;
		CheckPixel(RenderPicture(Test), ShadowColumn, Middle, Lit);
		CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 0);

		Sun->bCastShadows = true;
		Scene.Occluder->bCastShadows = false;
		CheckPixel(RenderPicture(Test), ShadowColumn, Middle, Lit);

		// A mesh that is not drawn casts no shadow either.
		Scene.Occluder->bCastShadows = true;
		Scene.Occluder->bVisible = false;
		CheckPixel(RenderPicture(Test), ShadowColumn, Middle, Lit);

		Scene.Occluder->bVisible = true;
		CHECK(RenderPicture(Test).GetPixel(ShadowColumn, Middle) == Black);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A surface casts a shadow whichever side it turns to the sun")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight);
		// Turned round, the occluder shows the camera, and the sun, its back, which is not drawn.
		Scene.Occluder->GetOwner()->SetRelativeTransform(MakeTransform(FVector(0.0, 0.0, -2.0), Turn(180.0f, AxisY)));

		const FImage Image = RenderPicture(Test);

		CHECK(Image.GetPixel(ShadowColumn, Middle) == Black);
		// Where the occluder would be, the wall behind it shows.
		CheckPixel(Image, Middle, Middle, GetLitWallPixel(Middle, Middle, SunFromTheRight));
	}

	TEST_CASE("Each sun has its own shadows, and a shadow takes away only its own sun's light")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight, FLinearColor(1.0f, 0.0f, 0.0f));
		AddSun(*Test.Scene.Get(), SunFromTheLeft, FLinearColor(0.0f, 0.0f, 1.0f));

		const FImage Image = RenderPicture(Test);

		// In the red sun's shadow only the blue sun shines, and the other way round.
		const FColor LeftPixel = Image.GetPixel(ShadowColumn, Middle);
		const FColor RightPixel = Image.GetPixel(LitColumn, Middle);
		CHECK(LeftPixel.R <= 2);
		CHECK(LeftPixel.B > 60);
		CHECK(RightPixel.B <= 2);
		CHECK(RightPixel.R > 60);
		CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 8);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A sky light and a lamp shine into a sun's shadow")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight);
		const FReferenceColor SkyLight(0.2, 0.3, 0.4);
		WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
		std::string Error;
		REQUIRE(Environment->SetImage(
			FHdrImage(32,
				16,
				FLinearColor(
					static_cast<float>(SkyLight.r), static_cast<float>(SkyLight.g), static_cast<float>(SkyLight.b))),
			Error));
		Test.Scene->CreateEntity("Sky")->AddComponent<WSkyLightComponent>()->Environment = Environment;

		const FImage Image = RenderPicture(Test);

		// In the shadow, the wall has the sky's light and nothing of the sun's.
		const double Across = (ShadowColumn + 0.5) / (0.5 * PictureSize) - 1.0;
		FReferenceSurface AtShadow;
		AtShadow.ToCamera = glm::normalize(-FVector(Across * 4.0, 0.0, -4.0));
		CheckPixel(Image, ShadowColumn, Middle, ReferencePixel(ReferenceReflectEvenEnvironment(AtShadow, SkyLight)));
		CHECK(Image.GetPixel(LitColumn, Middle).R > Image.GetPixel(ShadowColumn, Middle).R + 40);
	}

	TEST_CASE("The size of the shadow maps can be set, and impossible sizes are taken as the nearest possible one")
	{
		for (const int32 MapSize : {512, 4096, 0, -5, 1000000})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			const FShadowScene Scene(*Test.Scene.Get(), 1.0);
			AddSun(*Test.Scene.Get(), SunFromTheRight);
			Test.Renderer->ShadowMapSize = MapSize;

			const FImage Image = RenderPicture(Test);

			CAPTURE(MapSize);
			// The smallest map there is has pixels almost a metre wide here, too coarse to hold this shadow at
			// all; the picture is drawn all the same, and what is lit is lit correctly.
			if (MapSize >= 512)
			{
				CHECK(Image.GetPixel(ShadowColumn, Middle) == Black);
			}
			CheckPixel(Image, LitColumn, Middle, GetLitWallPixel(LitColumn, Middle, SunFromTheRight));
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("There are no shadows in the unlit view mode, and nothing is drawn for them when nothing casts any")
	{
		FSceneRenderTest Test(ESceneViewMode::Unlit);
		const FShadowScene Scene(*Test.Scene.Get(), 1.0);
		AddSun(*Test.Scene.Get(), SunFromTheRight);

		Test.Render(MakeWideView());
		CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 0);

		Test.Renderer->ViewMode = ESceneViewMode::Lit;
		Scene.Wall->bCastShadows = false;
		Scene.Occluder->bCastShadows = false;
		Test.Render(MakeWideView());
		CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 0);
	}

	TEST_CASE("Shadows fall the same a thousand and ten billion kilometres from the world origin")
	{
		std::vector<FImage> Images;
		for (const double Distance : {0.0, 1.0e6, 1.0e13})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			const FVector Origin(Distance, -Distance, Distance);
			const FShadowScene Scene(*Test.Scene.Get(), 1.0, Origin);
			AddSun(*Test.Scene.Get(), SunFromTheRight);

			const FImage Image = RenderPicture(Test, MakeWideView(Origin));

			CAPTURE(Distance);
			CHECK(Image.GetPixel(ShadowColumn, Middle) == Black);
			CheckPixel(Image, LitColumn, Middle, GetLitWallPixel(LitColumn, Middle, SunFromTheRight));
			Images.push_back(Image);
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}

		// The shadow maps lie on a grid that is fixed in the world, so the soft edge of the shadow may differ by a
		// shade from place to place. Nothing else may.
		for (std::size_t Index = 1; Index < Images.size(); ++Index)
		{
			const FImageDifference Difference = FImageDifference::Measure(Images[Index], Images[0], 6);
			INFO("Difference: ", Difference.Describe());
			CHECK(Difference.IsWithin(0.02));
			CHECK(Difference.LargestDifference < 128);
		}
	}
}
