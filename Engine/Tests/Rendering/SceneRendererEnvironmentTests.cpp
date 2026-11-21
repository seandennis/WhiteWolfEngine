#include "Rendering/EnvironmentMap.h"
#include "Rendering/HdrImage.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SkyLightComponent.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneLighting.h"
#include "Rendering/TestSceneRendering.h"
#include "Rendering/Texture.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <memory>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** What the directional test environment shows in each of the six directions along the axes. */
	const FReferenceColor LightAbove(0.2, 0.4, 1.0);
	const FReferenceColor LightBelow(0.4, 0.25, 0.1);
	const FReferenceColor LightAhead(1.0, 0.0, 0.0);
	const FReferenceColor LightRight(0.0, 1.0, 0.0);
	const FReferenceColor LightBehind(1.0, 1.0, 0.0);
	const FReferenceColor LightLeft(1.0, 0.0, 1.0);

	FLinearColor ToLinearColor(const FReferenceColor& Color)
	{
		return FLinearColor(static_cast<float>(Color.r), static_cast<float>(Color.g), static_cast<float>(Color.b));
	}

	WEnvironmentMap* MakeEnvironment(const FHdrImage& Image)
	{
		WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
		std::string Error;
		REQUIRE(Environment->SetImage(Image, Error));
		return Environment;
	}

	/** An environment with the same light in every direction. */
	WEnvironmentMap* MakeEvenEnvironment(const FReferenceColor& Light)
	{
		return MakeEnvironment(FHdrImage(64, 32, ToLinearColor(Light)));
	}

	/**
	 * An environment with one colour for each of the six directions along the axes: everything within 45 degrees
	 * of straight up is one colour and of straight down another, and the band in between is divided into the
	 * quarter turn around ahead, right, behind and left.
	 */
	FHdrImage MakeDirectionalImage()
	{
		constexpr int32 Width = 256;
		constexpr int32 Height = 128;
		FHdrImage Image(Width, Height);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				FReferenceColor Light = LightBehind;
				if (Y < Height / 4)
				{
					Light = LightAbove;
				}
				else if (Y >= Height * 3 / 4)
				{
					Light = LightBelow;
				}
				else if (X >= Width * 3 / 8 && X < Width * 5 / 8)
				{
					Light = LightAhead;
				}
				else if (X >= Width * 5 / 8 && X < Width * 7 / 8)
				{
					Light = LightRight;
				}
				else if (X >= Width / 8 && X < Width * 3 / 8)
				{
					Light = LightLeft;
				}
				Image.SetPixel(X, Y, ToLinearColor(Light));
			}
		}
		return Image;
	}

	WSkyLightComponent* AddSkyLight(WScene& Scene,
		WEnvironmentMap* Environment,
		float Intensity = 1.0f,
		const FQuat& Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f))
	{
		WEntity* const Entity = Scene.CreateEntity("Sky");
		REQUIRE(Entity != nullptr);
		Entity->SetRelativeTransform(MakeTransform(FVector(0.0), Rotation));

		WSkyLightComponent* const Light = Entity->AddComponent<WSkyLightComponent>();
		REQUIRE(Light != nullptr);
		Light->Environment = Environment;
		Light->Intensity = Intensity;
		return Light;
	}

	/** A camera at the origin with a narrow view, so that the whole picture lies well inside one part of the sky. */
	FCameraView MakeNarrowView(const FQuat& Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f))
	{
		FCameraView View;
		View.Rotation = Rotation;
		View.VerticalFieldOfView = 40.0f;
		return View;
	}
}

TEST_SUITE("FSceneRenderer environment")
{
	TEST_CASE("In an even environment a surface is lit as the model says, whatever it is made of")
	{
		struct FCase
		{
			FReferenceColor BaseColor;
			double Metallic;
			double Roughness;
		};

		const FReferenceColor Light(0.6, 0.5, 0.4);
		for (const FCase& Case : {
				 FCase{FReferenceColor(0.5), 0.0, 1.0},
				 FCase{FReferenceColor(0.9, 0.3, 0.1), 0.0, 0.5},
				 FCase{FReferenceColor(0.9, 0.3, 0.1), 0.0, 0.0},
				 FCase{FReferenceColor(1.0, 0.77, 0.34), 1.0, 0.0},
				 FCase{FReferenceColor(1.0, 0.77, 0.34), 1.0, 0.4},
				 FCase{FReferenceColor(1.0, 0.77, 0.34), 1.0, 1.0},
				 FCase{FReferenceColor(0.2, 0.6, 0.9), 0.5, 0.7},
			 })
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			FReferenceSurface Surface;
			Surface.BaseColor = Case.BaseColor;
			Surface.Metallic = Case.Metallic;
			Surface.Roughness = Case.Roughness;
			AddSurface(*Test.Scene.Get(), Surface);
			AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(Light));

			const FImage Image = RenderPicture(Test);

			CAPTURE(Case.Metallic);
			CAPTURE(Case.Roughness);
			CheckPixel(Image, Middle, Middle, ReferencePixel(ReferenceReflectEvenEnvironment(Surface, Light)));
			CHECK(Test.Renderer->GetStatistics().SkyLightCount == 1);
			CHECK(Test.Renderer->GetStatistics().UploadedEnvironmentCount == 1);
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("The intensity of a sky light scales its light and its sky")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		const FReferenceColor Light(0.3, 0.3, 0.3);
		AddSurface(*Test.Scene.Get(), Surface);
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(Light), 2.5f);

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image, Middle, Middle, ReferencePixel(ReferenceReflectEvenEnvironment(Surface, Light * 2.5)));
		CheckPixel(Image, 2, 2, ReferencePixel(Light * 2.5));
	}

	TEST_CASE("The light of the sky adds to the light of lamps and suns")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		const FReferenceColor Light(0.1, 0.2, 0.3);
		AddSurface(*Test.Scene.Get(), Surface);
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(Light));
		AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f), 1.5f);

		const FImage Image = RenderPicture(Test);

		const FReferenceColor Expected = ReferenceReflectEvenEnvironment(Surface, Light) +
			ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * 1.5);
		CheckPixel(Image, Middle, Middle, ReferencePixel(Expected));
	}

	TEST_CASE("The sky is shown behind the scene, the right way round")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		Test.Renderer->BackgroundColor = FLinearColor(0.0f, 5.0f, 0.0f);
		AddSkyLight(*Test.Scene.Get(), MakeEnvironment(MakeDirectionalImage()));

		struct FCase
		{
			const char* Looking;
			FQuat Rotation;
			FReferenceColor Light;
		};

		// Looking along each axis shows the colour of that direction, over the whole picture.
		for (const FCase& Case : {
				 FCase{"ahead", FQuat(1.0f, 0.0f, 0.0f, 0.0f), LightAhead},
				 FCase{"to the right", Turn(-90.0f, AxisY), LightRight},
				 FCase{"to the left", Turn(90.0f, AxisY), LightLeft},
				 FCase{"behind", Turn(180.0f, AxisY), LightBehind},
				 FCase{"up", Turn(90.0f, AxisX), LightAbove},
				 FCase{"down", Turn(-90.0f, AxisX), LightBelow},
			 })
		{
			const FImage Image = RenderPicture(Test, MakeNarrowView(Case.Rotation));

			CAPTURE(Case.Looking);
			const FColor Expected = ReferencePixel(Case.Light);
			CheckPixel(Image, Middle, Middle, Expected);
			CheckPixel(Image, 3, 3, Expected);
			CheckPixel(Image, PictureSize - 4, PictureSize - 4, Expected);
		}

		// Looking half way between ahead and up, the upper part of the picture is the sky above.
		const FImage TiltedUp = RenderPicture(Test, MakeNarrowView(Turn(45.0f, AxisX)));
		CheckPixel(TiltedUp, Middle, 8, ReferencePixel(LightAbove));
		CheckPixel(TiltedUp, Middle, PictureSize - 9, ReferencePixel(LightAhead));

		// Looking half way between ahead and right, the right part of the picture is what is to the right.
		const FImage TurnedRight = RenderPicture(Test, MakeNarrowView(Turn(-45.0f, AxisY)));
		CheckPixel(TurnedRight, 8, Middle, ReferencePixel(LightAhead));
		CheckPixel(TurnedRight, PictureSize - 9, Middle, ReferencePixel(LightRight));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Surfaces hide the sky, and the sky is as bright as the camera's exposure makes it")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(0.0);
		Surface.EmissiveColor = FReferenceColor(0.0, 0.5, 0.0);
		AddSurface(*Test.Scene.Get(), Surface);
		const FReferenceColor Light(4.0, 2.0, 1.0);
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(Light));
		FCameraView View = MakeWideView();
		View.Exposure = 0.25f;

		const FImage Image = RenderPicture(Test, View);

		CheckPixel(Image, 2, 2, ReferencePixel(Light, 0.25));
		// The black, glowing square shows its own light and what little of the sky it mirrors, not the sky behind.
		const FReferenceColor Expected = Surface.EmissiveColor + ReferenceReflectEvenEnvironment(Surface, Light);
		CheckPixel(Image, Middle, Middle, ReferencePixel(Expected, 0.25));
	}

	TEST_CASE("A mirror shows the part of the environment it faces")
	{
		struct FCase
		{
			const char* Mirroring;
			FQuat Rotation;
			FReferenceColor Light;
		};

		// The camera looks along -Z at a mirror two metres ahead. Facing the camera it shows what is behind the
		// camera; tilted by 45 degrees it shows what is above, below, to the right or to the left.
		for (const FCase& Case : {
				 FCase{"what is behind the camera", FQuat(1.0f, 0.0f, 0.0f, 0.0f), LightBehind},
				 FCase{"what is above", Turn(-45.0f, AxisX), LightAbove},
				 FCase{"what is below", Turn(45.0f, AxisX), LightBelow},
				 FCase{"what is to the right", Turn(45.0f, AxisY), LightRight},
				 FCase{"what is to the left", Turn(-45.0f, AxisY), LightLeft},
			 })
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			FReferenceSurface Mirror;
			Mirror.BaseColor = FReferenceColor(1.0);
			Mirror.Metallic = 1.0;
			Mirror.Roughness = 0.0;
			Mirror.Normal = FVector(Case.Rotation * AxisZ);
			AddSurface(*Test.Scene.Get(), Mirror, MakeTransform(SquareLocation, Case.Rotation));
			AddSkyLight(*Test.Scene.Get(), MakeEnvironment(MakeDirectionalImage()));

			const FImage Image = RenderPicture(Test);

			// A metal gathers no light as a matte surface would; all it shows is the mirror image.
			CAPTURE(Case.Mirroring);
			CheckPixel(Image,
				Middle,
				Middle,
				ReferencePixel(ReferenceReflectEnvironment(Mirror, FReferenceColor(0.0), Case.Light)));
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("A matte surface gathers the light of the half of the environment it faces")
	{
		// Bright behind the camera, which is what the test square faces, and dark everywhere else.
		FHdrImage Image(256, 128, FLinearColor(0.0f, 0.0f, 0.0f));
		for (int32 Y = 32; Y < 96; ++Y)
		{
			for (int32 X = 0; X < 256; ++X)
			{
				const bool bIsBehind = X < 32 || X >= 224;
				if (bIsBehind)
				{
					Image.SetPixel(X, Y, FLinearColor(4.0f, 4.0f, 4.0f));
				}
			}
		}
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(1.0);
		AddSurface(*Test.Scene.Get(), Surface);
		WEnvironmentMap* const Environment = MakeEnvironment(Image);
		WSkyLightComponent* const Sky = AddSkyLight(*Test.Scene.Get(), Environment);

		const FColor Facing = RenderPicture(Test).GetPixel(Middle, Middle);

		// What the surface gathers is what the environment map says it gathers for that direction.
		const FVector3f Gathered = Environment->GetIrradiance().Evaluate(FVector3f(0.0f, 0.0f, 1.0f));
		const FReferenceColor Diffuse =
			ReferenceReflectEnvironment(Surface, FReferenceColor(Gathered), FReferenceColor(0.0));
		const FColor DiffuseOnly = ReferencePixel(Diffuse);
		CHECK(Facing.R >= DiffuseOnly.R - PixelTolerance);
		CHECK(Facing.R <= DiffuseOnly.R + 12);
		CHECK(Facing.R > 100);

		// With the sky turned half way round, the bright part is behind the square, which then gathers next to nothing.
		Sky->GetOwner()->SetRelativeTransform(MakeTransform(FVector(0.0), Turn(180.0f, AxisY)));
		const FColor FacingAway = RenderPicture(Test).GetPixel(Middle, Middle);
		CHECK(FacingAway.R < Facing.R / 4);
	}

	TEST_CASE("Turning the sky light's entity turns the environment, for the sky, for mirrors and for matte surfaces")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Mirror;
		Mirror.BaseColor = FReferenceColor(1.0);
		Mirror.Metallic = 1.0;
		Mirror.Roughness = 0.0;
		AddSurface(*Test.Scene.Get(), Mirror);
		// Turned a quarter turn to the left: what was ahead is now to the left, and what was to the right is ahead.
		AddSkyLight(*Test.Scene.Get(), MakeEnvironment(MakeDirectionalImage()), 1.0f, Turn(90.0f, AxisY));

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image, 2, 2, ReferencePixel(LightRight));
		// The mirror shows what is behind the camera, which is what used to be to the left.
		CheckPixel(Image,
			Middle,
			Middle,
			ReferencePixel(ReferenceReflectEnvironment(Mirror, FReferenceColor(0.0), LightLeft)));
	}

	TEST_CASE("Sky lights that are invisible, without intensity, without a picture or destroyed do not shine")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		Test.Renderer->BackgroundColor = FLinearColor(0.0f, 0.0f, 1.0f);
		AddSurface(*Test.Scene.Get(), FReferenceSurface());
		WEnvironmentMap* const Environment = MakeEvenEnvironment(FReferenceColor(1.0));
		WSkyLightComponent* const Hidden = AddSkyLight(*Test.Scene.Get(), Environment);
		WSkyLightComponent* const Dark = AddSkyLight(*Test.Scene.Get(), Environment);
		WSkyLightComponent* const Empty = AddSkyLight(*Test.Scene.Get(), NewObject<WEnvironmentMap>());
		WSkyLightComponent* const WithoutEnvironment = AddSkyLight(*Test.Scene.Get(), nullptr);
		WSkyLightComponent* const Destroyed = AddSkyLight(*Test.Scene.Get(), Environment);
		CHECK(RenderPicture(Test).GetPixel(Middle, Middle) != Black);
		CHECK(Test.Renderer->GetStatistics().SkyLightCount == 1);

		Hidden->bVisible = false;
		Dark->Intensity = 0.0f;
		Destroyed->Destroy();

		// Without a sky light the surface is black and the background colour shows.
		const FImage Image = RenderPicture(Test);
		CHECK(Image.GetPixel(Middle, Middle) == Black);
		CheckPixel(Image, 2, 2, ReferencePixel(FReferenceColor(0.0, 0.0, 1.0)));
		CHECK(Test.Renderer->GetStatistics().SkyLightCount == 0);
		CHECK(Empty->bVisible);
		CHECK(WithoutEnvironment->bVisible);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A scene is lit by its first sky light")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		const FReferenceColor First(0.5, 0.0, 0.0);
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(First));
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(FReferenceColor(0.0, 0.0, 0.5)));

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image, Middle, Middle, ReferencePixel(ReferenceReflectEvenEnvironment(Surface, First)));
		CHECK(Test.Renderer->GetStatistics().SkyLightCount == 1);
		CHECK(Test.Renderer->GetStatistics().CachedEnvironmentCount == 1);
	}

	TEST_CASE(
		"An environment is copied to the graphics card once, again when its picture changes, and released with it")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		WEnvironmentMap* const Environment = MakeEvenEnvironment(FReferenceColor(0.5));
		WSkyLightComponent* const Sky = AddSkyLight(*Test.Scene.Get(), Environment);

		RenderPicture(Test);
		CHECK(Test.Renderer->GetStatistics().UploadedEnvironmentCount == 1);
		RenderPicture(Test);
		CHECK(Test.Renderer->GetStatistics().UploadedEnvironmentCount == 0);
		CHECK(Test.Renderer->GetStatistics().CachedEnvironmentCount == 1);

		// A new picture, of another size, shows in the next picture drawn.
		const FReferenceColor NewLight(0.1, 0.6, 0.2);
		std::string Error;
		REQUIRE(Environment->SetImage(FHdrImage(16, 8, ToLinearColor(NewLight)), Error));
		const FImage Image = RenderPicture(Test);
		CheckPixel(Image, Middle, Middle, ReferencePixel(ReferenceReflectEvenEnvironment(Surface, NewLight)));
		CheckPixel(Image, 2, 2, ReferencePixel(NewLight));
		CHECK(Test.Renderer->GetStatistics().UploadedEnvironmentCount == 1);
		CHECK(Test.Renderer->GetStatistics().CachedEnvironmentCount == 1);

		Sky->GetOwner()->Destroy();
		CollectGarbage();
		RenderPicture(Test);
		CHECK(Test.Renderer->GetStatistics().CachedEnvironmentCount == 0);
		CHECK(Test.Renderer->GetStatistics().SkyLightCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE(
		"An environment far brighter than a display, with a sun in it, lights a scene without breaking the picture")
	{
		// A dim sky with a small sun, straight above, that is brighter than a 16-bit float can hold.
		FHdrImage Image(128, 64, FLinearColor(0.2f, 0.3f, 0.5f));
		for (int32 X = 0; X < 128; ++X)
		{
			Image.SetPixel(X, 0, FLinearColor(200000.0f, 180000.0f, 150000.0f));
		}
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.Roughness = 0.3;
		AddSurface(*Test.Scene.Get(), Surface, MakeTransform(SquareLocation, Turn(-60.0f, AxisX)));
		AddSkyLight(*Test.Scene.Get(), MakeEnvironment(Image));
		FCameraView View = MakeWideView();
		View.Exposure = 0.05f;

		const FImage Picture = RenderPicture(Test, View);

		// Every pixel is a colour: nothing not-a-number, and alpha is whole.
		int32 BrokenPixelCount = 0;
		for (const FColor& Pixel : Picture.GetPixels())
		{
			BrokenPixelCount += Pixel.A == 255 ? 0 : 1;
		}
		CHECK(BrokenPixelCount == 0);
		CHECK(Picture.GetPixel(Middle, Middle) != Black);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("An occlusion texture dims the light of the sky and leaves the light of a sun alone")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		WStaticMeshComponent* const Component = AddSurface(*Test.Scene.Get(), Surface);
		// Fully hidden from its surroundings on the left, open on the right.
		FImage Occlusion(16, 16, White);
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 8; ++X)
			{
				Occlusion.SetPixel(X, Y, Black);
			}
		}
		WTexture* const Texture = NewObject<WTexture>();
		std::string Error;
		REQUIRE(Texture->SetImage(Occlusion, ETextureColorSpace::Linear, Error));
		Component->Material->OcclusionTexture = Texture;
		const FReferenceColor SkyLight(0.5, 0.5, 0.5);
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(SkyLight));

		const FReferenceColor FromSky = ReferenceReflectEvenEnvironment(Surface, SkyLight);
		const FImage OnlySky = RenderPicture(Test);
		CHECK(OnlySky.GetPixel(Middle - 4, Middle) == Black);
		CheckPixel(OnlySky, Middle + 4, Middle, ReferencePixel(FromSky));

		// At half strength the hidden side gets half the light.
		Component->Material->OcclusionStrength = 0.5f;
		CheckPixel(RenderPicture(Test), Middle - 4, Middle, ReferencePixel(FromSky * 0.5));

		// A sun lights both sides alike; only the sky's share differs.
		Component->Material->OcclusionStrength = 1.0f;
		AddSun(*Test.Scene.Get());
		const FReferenceColor FromSun =
			ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * std::numbers::pi);
		const FImage WithSun = RenderPicture(Test);
		CheckPixel(WithSun, Middle - 4, Middle, ReferencePixel(FromSun));
		CheckPixel(WithSun, Middle + 4, Middle, ReferencePixel(FromSun + FromSky));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("In the unlit view mode there is no sky and no light from it")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(SquareLocation), LinearGreen);
		AddSkyLight(*Test.Scene.Get(), MakeEvenEnvironment(FReferenceColor(5.0)));

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Green);
		CHECK(Image.GetPixel(2, 2) == Magenta);
		CHECK(Test.Renderer->GetStatistics().SkyLightCount == 0);
	}

	TEST_CASE("A scene lit by a sky light is the same a thousand and ten billion kilometres from the world origin")
	{
		std::vector<FImage> Images;
		for (const double Distance : {0.0, 1.0e6, 1.0e13})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			const FVector Origin(Distance, -Distance, -Distance);

			FReferenceSurface Gold;
			Gold.BaseColor = FReferenceColor(1.0, 0.77, 0.34);
			Gold.Metallic = 1.0;
			Gold.Roughness = 0.25;
			AddSurface(*Test.Scene.Get(), Gold, MakeTransform(Origin + FVector(-0.5, 0.0, -2.0), Turn(30.0f, AxisY)));

			FReferenceSurface Floor;
			Floor.BaseColor = FReferenceColor(0.6);
			AddSurface(*Test.Scene.Get(),
				Floor,
				MakeTransform(Origin + FVector(0.0, -0.5, -2.0), Turn(-90.0f, AxisX), FVector3f(6.0f)));

			// The sky light's entity is far from the origin too, which must not matter: only its rotation counts.
			WSkyLightComponent* const Sky =
				AddSkyLight(*Test.Scene.Get(), MakeEnvironment(MakeDirectionalImage()), 1.0f, Turn(20.0f, AxisY));
			Sky->GetOwner()->SetRelativeTransform(MakeTransform(Origin, Turn(20.0f, AxisY)));

			FCameraView View = MakeWideView(Origin + FVector(0.0, 0.25, 0.0));
			View.Rotation = Turn(-10.0f, AxisX);
			Images.push_back(Test.Render(View, 96, 64));
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}

		CHECK(Images[1] == Images[0]);
		CHECK(Images[2] == Images[0]);
	}
}
