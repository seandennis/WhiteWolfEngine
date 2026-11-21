#include "Logging/LogMessage.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneLighting.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

// These tests light a square and compare the pixel in the middle of the picture with what the lighting model
// says it must be; see TestSceneLighting.h for the model.

namespace
{
	int32 CountWarnings(const FScopedLogCapture& Capture)
	{
		int32 Count = 0;
		for (const FLogMessage& Message : Capture.GetMessages())
		{
			Count += Message.Verbosity == ELogVerbosity::Warning ? 1 : 0;
		}
		return Count;
	}
}

TEST_SUITE("FSceneRenderer lighting")
{
	TEST_CASE("Without lights a surface shows only the light it gives off itself")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Glowing;
		Glowing.EmissiveColor = FReferenceColor(0.5, 0.25, 0.0);
		AddSurface(*Test.Scene.Get(), Glowing, MakeTransform(FVector(-1.0, 0.0, -2.0)));
		AddSurface(*Test.Scene.Get(), FReferenceSurface(), MakeTransform(FVector(1.0, 0.0, -2.0)));

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image, 16, Middle, ReferencePixel(Glowing.EmissiveColor));
		CHECK(Image.GetPixel(48, Middle) == Black);
		CHECK(Image.GetPixel(Middle, 2) == Black);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 2);
		CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == 0);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A directional light that faces a matte surface lights it as the model says")
	{
		for (const FReferenceColor& BaseColor :
			{FReferenceColor(0.5), FReferenceColor(1.0), FReferenceColor(0.8, 0.4, 0.1), FReferenceColor(0.0)})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			FReferenceSurface Surface;
			Surface.BaseColor = BaseColor;
			AddSurface(*Test.Scene.Get(), Surface);
			AddSun(*Test.Scene.Get());

			const FImage Image = RenderPicture(Test);

			const FVector ToLight(0.0, 0.0, 1.0);
			CheckPixel(Image,
				Middle,
				Middle,
				ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
			CHECK(Image.GetPixel(2, 2) == Black);
			CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == 1);
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("The default light lights a white matte surface to a brightness of about one")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		const WDirectionalLightComponent* const Sun =
			Scene->CreateEntity("Sun")->AddComponent<WDirectionalLightComponent>();
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(1.0);

		const FReferenceColor Light =
			ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * static_cast<double>(Sun->Intensity));

		CHECK(Light.r == doctest::Approx(0.97).epsilon(0.01));
	}

	TEST_CASE("The colour and intensity of a light scale what it adds")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FLinearColor(1.0f, 0.5f, 0.25f), 4.5f);

		const FImage Image = RenderPicture(Test);

		const FReferenceColor Illuminance = FReferenceColor(1.0, 0.5, 0.25) * 4.5;
		CheckPixel(
			Image, Middle, Middle, ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), Illuminance)));
	}

	TEST_CASE("Light that arrives at an angle lights less, and light from behind not at all")
	{
		const FReferenceSurface Surface;
		for (const float Degrees : {30.0f, 60.0f, 85.0f, 95.0f, 180.0f})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			AddSurface(*Test.Scene.Get(), Surface);
			const FQuat Rotation = Turn(Degrees, AxisY);
			AddSun(*Test.Scene.Get(), Rotation);

			const FImage Image = RenderPicture(Test);

			// A light shines along its entity's -Z axis.
			const FVector ToLight = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
			CAPTURE(Degrees);
			CheckPixel(Image,
				Middle,
				Middle,
				ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
			if (Degrees > 90.0f)
			{
				CHECK(Image.GetPixel(Middle, Middle) == Black);
			}
		}
	}

	TEST_CASE("A light shines the way its entity faces in the world, parents included")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		// The parent turns the light away from the square, and the light's own rotation turns it back by half.
		WEntity* const Parent = Test.Scene->CreateEntity("Parent");
		Parent->SetRelativeTransform(MakeTransform(FVector(100.0, 0.0, 0.0), Turn(120.0f, AxisX)));
		AddSun(
			*Test.Scene.Get(), Turn(-60.0f, AxisX), FLinearColor(1.0f, 1.0f, 1.0f), std::numbers::pi_v<float>, Parent);

		const FImage Image = RenderPicture(Test);

		const FVector ToLight = -FVector(Turn(60.0f, AxisX) * FVector3f(0.0f, 0.0f, -1.0f));
		CheckPixel(Image,
			Middle,
			Middle,
			ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
		CHECK(Image.GetPixel(Middle, Middle) != Black);
	}

	TEST_CASE("A surface that is turned is lit by the way it then faces")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		const FQuat Rotation = Turn(50.0f, AxisY);
		Surface.Normal = FVector(Rotation * AxisZ);
		AddSurface(*Test.Scene.Get(), Surface, MakeTransform(SquareLocation, Rotation, FVector3f(2.0f)));
		AddSun(*Test.Scene.Get());

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image,
			Middle,
			Middle,
			ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * std::numbers::pi)));
	}

	TEST_CASE("A stretched mesh is lit by the way its surface really faces")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		// A square that is tilted 45 degrees within its own mesh, then stretched to twice its width by its
		// entity. Stretching makes the surface face the camera more squarely, so its normal turns towards +Z:
		// from (1, 0, 1) to (1, 0, 2), not to (2, 0, 1) as it would if normals were scaled like positions.
		const float Half = 0.5f;
		const FVector3f MeshNormal = glm::normalize(FVector3f(1.0f, 0.0f, 1.0f));
		const std::vector<FStaticMeshVertex> Vertices{
			{FVector3f(-Half, -Half, Half), MeshNormal},
			{FVector3f(Half, -Half, -Half), MeshNormal},
			{FVector3f(Half, Half, -Half), MeshNormal},
			{FVector3f(-Half, Half, Half), MeshNormal},
		};
		FReferenceSurface Surface;
		Surface.Normal = glm::normalize(FVector(1.0, 0.0, 2.0));
		WStaticMeshComponent* const Component = AddSurface(*Test.Scene.Get(),
			Surface,
			MakeTransform(SquareLocation, FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(2.0f, 1.0f, 1.0f)));
		std::string Error;
		REQUIRE(Component->Mesh->SetGeometry(Vertices, {0, 1, 2, 0, 2, 3}, Error));
		AddSun(*Test.Scene.Get());

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image,
			Middle,
			Middle,
			ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * std::numbers::pi)));
	}

	TEST_CASE("A point light gets weaker with the square of the distance")
	{
		const FReferenceSurface Surface;
		for (const double Distance : {0.5, 1.0, 2.0, 8.0})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			AddSurface(*Test.Scene.Get(), Surface);
			const float Intensity = 6.0f;
			AddLamp(*Test.Scene.Get(), SquareLocation + FVector(0.0, 0.0, Distance), Intensity);

			const FImage Image = RenderPicture(Test);

			const FReferenceColor Illuminance =
				ReferenceWhite * (static_cast<double>(Intensity) / (Distance * Distance));
			CAPTURE(Distance);
			CheckPixel(
				Image, Middle, Middle, ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), Illuminance)));
			CHECK(Test.Renderer->GetStatistics().PointLightCount == 1);
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("A point light lights each point from where the light is")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		// To the side of the square's middle and a little in front of it.
		const FVector Offset(1.5, 0.0, 0.5);
		const float Intensity = 3.0f;
		AddLamp(*Test.Scene.Get(), SquareLocation + Offset, Intensity);

		const FImage Image = RenderPicture(Test);

		const FReferenceColor Illuminance =
			ReferenceWhite * (static_cast<double>(Intensity) / glm::dot(Offset, Offset));
		CheckPixel(
			Image, Middle, Middle, ReferencePixel(ReferenceReflect(Surface, glm::normalize(Offset), Illuminance)));
		// The side of the square nearer to the light is brighter than the far side.
		CHECK(Image.GetPixel(39, Middle).R > Image.GetPixel(25, Middle).R + 10);
	}

	TEST_CASE("A point light with a range fades out towards it and lights nothing beyond it")
	{
		const FReferenceSurface Surface;
		const float Intensity = 3.0f;

		for (const float Range : {4.0f, 2.0f, 1.25f})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			AddSurface(*Test.Scene.Get(), Surface);
			AddLamp(*Test.Scene.Get(), SquareLocation + FVector(0.0, 0.0, 1.0), Intensity, Range);

			const FImage Image = RenderPicture(Test);

			// One metre from the light, the window glTF recommends leaves this much of the light.
			const double Ratio = 1.0 / static_cast<double>(Range * Range);
			const double Window = std::clamp(1.0 - Ratio * Ratio, 0.0, 1.0);
			const FReferenceColor Illuminance = ReferenceWhite * (static_cast<double>(Intensity) * Window * Window);
			CAPTURE(Range);
			CheckPixel(
				Image, Middle, Middle, ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), Illuminance)));
		}

		for (const float Range : {1.0f, 0.5f})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			AddSurface(*Test.Scene.Get(), Surface);
			AddLamp(*Test.Scene.Get(), SquareLocation + FVector(0.0, 0.0, 1.0), Intensity, Range);

			CAPTURE(Range);
			CHECK(RenderPicture(Test).GetPixel(Middle, Middle) == Black);
		}
	}

	TEST_CASE("Metals and rough or smooth surfaces reflect as the model says")
	{
		struct FCase
		{
			FReferenceColor BaseColor;
			double Metallic;
			double Roughness;
		};

		for (const FCase& Case : {
				 FCase{FReferenceColor(1.0, 0.6, 0.2), 1.0, 1.0},
				 FCase{FReferenceColor(1.0, 0.6, 0.2), 1.0, 0.6},
				 FCase{FReferenceColor(0.2, 0.4, 0.8), 0.5, 0.8},
				 FCase{FReferenceColor(0.2, 0.4, 0.8), 0.0, 0.5},
				 FCase{FReferenceColor(0.1, 0.1, 0.1), 0.0, 0.35},
			 })
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			FReferenceSurface Surface;
			Surface.BaseColor = Case.BaseColor;
			Surface.Metallic = Case.Metallic;
			Surface.Roughness = Case.Roughness;
			AddSurface(*Test.Scene.Get(), Surface);
			// From a little to the side, so that the picture's middle is not the peak of the highlight.
			const FQuat Rotation = Turn(25.0f, AxisY);
			AddSun(*Test.Scene.Get(), Rotation, FLinearColor(1.0f, 1.0f, 1.0f), 2.0f);

			const FImage Image = RenderPicture(Test);

			const FVector ToLight = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
			CAPTURE(Case.Metallic);
			CAPTURE(Case.Roughness);
			CheckPixel(Image, Middle, Middle, ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * 2.0)));
		}
	}

	TEST_CASE("Metallic and roughness values outside their range are treated as the nearest valid value")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(0.6, 0.3, 0.1);
		Surface.Metallic = 7.0;
		Surface.Roughness = -3.0;
		AddSurface(*Test.Scene.Get(), Surface);
		const FQuat Rotation = Turn(40.0f, AxisY);
		AddSun(*Test.Scene.Get(), Rotation);

		const FImage Image = RenderPicture(Test);

		// The reference clamps too: a mirror-smooth metal, seen away from its highlight, is nearly black.
		const FVector ToLight = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
		CheckPixel(Image,
			Middle,
			Middle,
			ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
		CHECK(Image.GetPixel(Middle, Middle).A == 255);
	}

	TEST_CASE("Lights add up")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(0.3, 0.3, 0.3);
		Surface.EmissiveColor = FReferenceColor(0.0, 0.02, 0.05);
		AddSurface(*Test.Scene.Get(), Surface);
		const FQuat Rotation = Turn(-35.0f, AxisX);
		AddSun(*Test.Scene.Get(), Rotation, FLinearColor(1.0f, 0.0f, 0.0f), 1.5f);
		AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FLinearColor(0.0f, 1.0f, 0.0f), 0.5f);
		AddLamp(*Test.Scene.Get(), SquareLocation + FVector(0.0, 0.0, 2.0), 4.0f)->Color = LinearBlue;

		const FImage Image = RenderPicture(Test);

		const FVector Ahead(0.0, 0.0, 1.0);
		const FVector ToTiltedSun = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
		const FReferenceColor Light = Surface.EmissiveColor +
			ReferenceReflect(Surface, ToTiltedSun, FReferenceColor(1.5, 0.0, 0.0)) +
			ReferenceReflect(Surface, Ahead, FReferenceColor(0.0, 0.5, 0.0)) +
			ReferenceReflect(Surface, Ahead, FReferenceColor(0.0, 0.0, 4.0 / 4.0));
		CheckPixel(Image, Middle, Middle, ReferencePixel(Light));
		CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == 2);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The camera's exposure scales the light before it is fitted to the display")
	{
		const FReferenceSurface Surface;
		for (const float Exposure : {0.125f, 0.5f, 1.0f, 3.0f})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			AddSurface(*Test.Scene.Get(), Surface);
			AddSun(*Test.Scene.Get());
			FCameraView View = MakeWideView();
			View.Exposure = Exposure;

			const FImage Image = RenderPicture(Test, View);

			const FReferenceColor Light =
				ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * std::numbers::pi);
			CAPTURE(Exposure);
			CheckPixel(Image, Middle, Middle, ReferencePixel(Light, static_cast<double>(Exposure)));
		}
	}

	TEST_CASE("Sunlight in real units is shown correctly with a matching exposure")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(0.7, 0.5, 0.3);
		AddSurface(*Test.Scene.Get(), Surface);
		const float SunlightLux = 100000.0f;
		AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f), SunlightLux);
		FCameraView View = MakeWideView();
		View.Exposure = 3.0f / SunlightLux;

		const FImage Image = RenderPicture(Test, View);

		const FReferenceColor Light =
			ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * static_cast<double>(SunlightLux));
		CheckPixel(Image, Middle, Middle, ReferencePixel(Light, static_cast<double>(View.Exposure)));
	}

	TEST_CASE("Light far brighter than the display fades to white instead of breaking the picture")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.BaseColor = FReferenceColor(0.9, 0.2, 0.1);
		AddSurface(*Test.Scene.Get(), Surface);
		AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f), 1.0e6f);

		const FImage Image = RenderPicture(Test);

		const FColor Pixel = Image.GetPixel(Middle, Middle);
		CHECK(Pixel.R >= 250);
		CHECK(Pixel.G >= 240);
		CHECK(Pixel.B >= 240);
		CHECK(Pixel.A == 255);
		CHECK(Image.GetPixel(2, 2) == Black);
	}

	TEST_CASE("The background is light like any other in the lit view mode")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		Test.Renderer->BackgroundColor = FLinearColor(0.25f, 0.5f, 2.0f, 1.0f);
		FCameraView View = MakeWideView();
		View.Exposure = 0.5f;

		const FImage Image = RenderPicture(Test, View);

		CheckPixel(Image, Middle, Middle, ReferencePixel(FReferenceColor(0.25, 0.5, 2.0), 0.5));
		CheckPixel(Image, 0, PictureSize - 1, ReferencePixel(FReferenceColor(0.25, 0.5, 2.0), 0.5));
	}

	TEST_CASE("A component without a material is lit like one with a new material")
	{
		std::vector<FImage> Images;
		for (const bool bWithMaterial : {false, true})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			WStaticMeshComponent* const Component =
				AddSquare(*Test.Scene.Get(), MakeTransform(SquareLocation), LinearRed);
			Component->Material = bWithMaterial ? NewObject<WMaterial>() : nullptr;
			AddSun(*Test.Scene.Get(), Turn(20.0f, AxisY));
			AddLamp(*Test.Scene.Get(), FVector(0.5, 0.25, -1.0), 2.0f);

			Images.push_back(RenderPicture(Test));
		}

		CHECK(Images[0].GetPixel(Middle, Middle) != Black);
		CHECK(Images[0] == Images[1]);
	}

	TEST_CASE("Lights that are invisible, have no intensity or are destroyed do not shine")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		AddSurface(*Test.Scene.Get(), FReferenceSurface());
		WDirectionalLightComponent* const HiddenSun = AddSun(*Test.Scene.Get());
		WDirectionalLightComponent* const DarkSun = AddSun(*Test.Scene.Get());
		WDirectionalLightComponent* const DestroyedSun = AddSun(*Test.Scene.Get());
		WPointLightComponent* const HiddenLamp = AddLamp(*Test.Scene.Get(), FVector(0.0, 0.0, -1.0), 5.0f);
		WPointLightComponent* const NegativeLamp = AddLamp(*Test.Scene.Get(), FVector(0.0, 0.0, -1.0), 5.0f);
		WPointLightComponent* const DestroyedLamp = AddLamp(*Test.Scene.Get(), FVector(0.0, 0.0, -1.0), 5.0f);
		CHECK(RenderPicture(Test).GetPixel(Middle, Middle) != Black);
		CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == 3);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == 3);

		HiddenSun->bVisible = false;
		DarkSun->Intensity = 0.0f;
		DestroyedSun->GetOwner()->Destroy();
		HiddenLamp->bVisible = false;
		NegativeLamp->Intensity = -5.0f;
		DestroyedLamp->Destroy();

		CHECK(RenderPicture(Test).GetPixel(Middle, Middle) == Black);
		CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == 0);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A scene with more lights than a picture can have is lit by the first ones, with one warning")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		Test.Scene->SetName("Crowded");
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		const float Intensity = 0.25f;
		for (int32 Index = 0; Index < FSceneRenderer::MaxDirectionalLights + 2; ++Index)
		{
			AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f), Intensity);
		}
		for (int32 Index = 0; Index < FSceneRenderer::MaxPointLights + 3; ++Index)
		{
			AddLamp(*Test.Scene.Get(), SquareLocation + FVector(0.0, 0.0, 1.0), Intensity);
		}

		const FImage Image = RenderPicture(Test);
		RenderPicture(Test);

		const double UsedLights = FSceneRenderer::MaxDirectionalLights + FSceneRenderer::MaxPointLights;
		const FReferenceColor Illuminance = ReferenceWhite * (static_cast<double>(Intensity) * UsedLights);
		CheckPixel(
			Image, Middle, Middle, ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), Illuminance)));
		CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == FSceneRenderer::MaxDirectionalLights);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == FSceneRenderer::MaxPointLights);

		// The warning names the scene and how many lights were left out, and is not repeated for the second picture.
		REQUIRE(CountWarnings(Test.Capture) == 1);
		bool bFoundWarning = false;
		for (const FLogMessage& Message : Test.Capture.GetMessages())
		{
			const bool bIsTheWarning = Message.Verbosity == ELogVerbosity::Warning &&
				Message.Text.find("'Crowded'") != std::string::npos &&
				Message.Text.find("5 are ignored") != std::string::npos;
			bFoundWarning = bFoundWarning || bIsTheWarning;
		}
		CHECK(bFoundWarning);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The unlit view mode shows base colours exactly and ignores the lights")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		AddSquare(*Test.Scene.Get(), MakeTransform(SquareLocation), LinearGreen);
		AddSun(*Test.Scene.Get(), FQuat(1.0f, 0.0f, 0.0f, 0.0f), LinearRed, 50.0f);
		AddLamp(*Test.Scene.Get(), FVector(0.0, 0.0, -1.0), 50.0f);
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, PictureSize, PictureSize);
		FCameraView View = MakeWideView();
		View.Exposure = 0.01f;
		std::string Error;

		// The same target is drawn lit, then unlit, then lit again.
		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), View, *Target, Error));
		const FImage Lit = ReadTestTarget(*Target);

		Test.Renderer->ViewMode = ESceneViewMode::Unlit;
		Test.Renderer->BackgroundColor = LinearMagenta;
		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), View, *Target, Error));
		const FImage Unlit = ReadTestTarget(*Target);
		CHECK(Unlit.GetPixel(Middle, Middle) == Green);
		CHECK(Unlit.GetPixel(2, 2) == Magenta);
		CHECK(Test.Renderer->GetStatistics().DirectionalLightCount == 0);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == 0);

		Test.Renderer->ViewMode = ESceneViewMode::Lit;
		Test.Renderer->BackgroundColor = LinearBlack;
		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), View, *Target, Error));
		CHECK(ReadTestTarget(*Target) == Lit);
		CHECK(Lit.GetPixel(Middle, Middle) != Green);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Two renderers can light the same render target in turn")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		const FReferenceSurface Surface;
		AddSurface(*Test.Scene.Get(), Surface);
		AddSun(*Test.Scene.Get());
		std::string Error;
		const std::unique_ptr<FSceneRenderer> Second = FSceneRenderer::Create(*Test.Device, Error);
		REQUIRE(Second != nullptr);
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, PictureSize, PictureSize);
		const FColor Expected =
			ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * std::numbers::pi));

		for (int32 Round = 0; Round < 2; ++Round)
		{
			REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), MakeWideView(), *Target, Error));
			CheckPixel(ReadTestTarget(*Target), Middle, Middle, Expected);
			REQUIRE(Second->Render(*Test.Scene.Get(), MakeWideView(), *Target, Error));
			CheckPixel(ReadTestTarget(*Target), Middle, Middle, Expected);
		}
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A scene with more meshes than the renderer has drawn before is drawn completely")
	{
		FSceneRenderTest Test(ESceneViewMode::Unlit);
		// A grid of 20 by 20 small squares in four colours, which is more than the renderer's buffer of per-mesh
		// data holds at first, so the buffer has to grow.
		WStaticMesh* const Mesh = MakeSquareMesh();
		const int32 GridSize = 20;
		for (int32 Row = 0; Row < GridSize; ++Row)
		{
			for (int32 Column = 0; Column < GridSize; ++Column)
			{
				WEntity* const Entity = Test.Scene->CreateEntity("Tile");
				// The picture spans four metres at this distance: each tile is a fifth of a metre.
				Entity->SetRelativeTransform(MakeTransform(FVector(-1.9 + 0.2 * Column, 1.9 - 0.2 * Row, -2.0),
					FQuat(1.0f, 0.0f, 0.0f, 0.0f),
					FVector3f(0.2f)));
				WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
				Component->Mesh = Mesh;
				Component->Material =
					MakeMaterial(FLinearColor(Row % 2 == 0 ? 1.0f : 0.0f, Column % 2 == 0 ? 1.0f : 0.0f, 1.0f));
			}
		}

		// 200 pixels a side: every tile covers 10 by 10 pixels.
		const FImage Image = Test.Render(MakeWideView(), 200, 200);

		int32 WrongPixelCount = 0;
		for (int32 Row = 0; Row < GridSize; ++Row)
		{
			for (int32 Column = 0; Column < GridSize; ++Column)
			{
				const FColor Expected =
					Row % 2 == 0 ? (Column % 2 == 0 ? White : Magenta) : (Column % 2 == 0 ? FColor(0, 255, 255) : Blue);
				WrongPixelCount += Image.GetPixel(Column * 10 + 5, Row * 10 + 5) == Expected ? 0 : 1;
			}
		}
		CHECK(WrongPixelCount == 0);
		CHECK(Test.Renderer->GetStatistics() == MakeMeshStatistics(400, 1, 1));

		// A smaller scene afterwards still draws correctly from the larger buffer.
		Test.Scene->GetRootEntities().front()->Destroy();
		Test.Render(MakeWideView(), 200, 200);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 399);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The lit picture is the same at the origin, a thousand and ten billion kilometres from it")
	{
		// As in the unlit test of the same name, all offsets are multiples of an eighth of a metre, which double
		// precision represents exactly ten billion kilometres out. The lights are placed the same way, so the
		// lighting, which is calculated relative to the camera, must come out equal to the last bit.
		std::vector<FImage> Images;
		for (const double Distance : {0.0, 1.0e6, 1.0e13})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			const FVector Origin(-Distance, Distance, Distance);

			FReferenceSurface Plastic;
			Plastic.BaseColor = FReferenceColor(0.8, 0.1, 0.1);
			Plastic.Roughness = 0.4;
			AddSurface(
				*Test.Scene.Get(), Plastic, MakeTransform(Origin + FVector(-0.5, 0.0, -2.0), Turn(25.0f, AxisY)));

			FReferenceSurface Gold;
			Gold.BaseColor = FReferenceColor(1.0, 0.77, 0.34);
			Gold.Metallic = 1.0;
			Gold.Roughness = 0.3;
			AddSurface(*Test.Scene.Get(), Gold, MakeTransform(Origin + FVector(0.75, 0.25, -2.5), Turn(-30.0f, AxisY)));

			FReferenceSurface Floor;
			Floor.BaseColor = FReferenceColor(0.5);
			AddSurface(*Test.Scene.Get(),
				Floor,
				MakeTransform(Origin + FVector(0.0, -0.5, -2.0), Turn(-90.0f, AxisX), FVector3f(6.0f)));

			AddSun(*Test.Scene.Get(), Turn(-50.0f, AxisX) * Turn(20.0f, AxisY), FLinearColor(1.0f, 0.95f, 0.9f), 2.0f);
			AddLamp(*Test.Scene.Get(), Origin + FVector(0.25, 0.75, -1.5), 3.0f)->Color =
				FLinearColor(0.4f, 0.6f, 1.0f);
			AddLamp(*Test.Scene.Get(), Origin + FVector(-1.0, 0.125, -1.0), 2.0f, 4.0f);

			FCameraView View = MakeWideView(Origin + FVector(0.0, 0.25, 0.0));
			View.Rotation = Turn(-10.0f, AxisX);
			Images.push_back(Test.Render(View, 96, 64));

			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}

		// The picture is not equal by being empty or flat: it has many different colours.
		std::vector<FColor> DistinctColors;
		for (const FColor& Pixel : Images[0].GetPixels())
		{
			if (std::ranges::find(DistinctColors, Pixel) == DistinctColors.end())
			{
				DistinctColors.push_back(Pixel);
			}
		}
		CHECK(DistinctColors.size() > 200);

		CHECK(Images[1] == Images[0]);
		CHECK(Images[2] == Images[0]);
	}
}
