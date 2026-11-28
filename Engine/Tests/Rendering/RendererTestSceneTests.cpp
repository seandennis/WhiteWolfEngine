#include "Core/FileHelper.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/Image.h"
#include "Rendering/ImageDifference.h"
#include "Rendering/RendererTestScene.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SkyLightComponent.h"
#include "Rendering/TestReferenceImages.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

// The renderer's test scene is drawn without a window and compared with a reference image that is stored in the
// repository (Rendering/TestReferenceImages.h).

namespace
{
	/**
	 * The limits of Rendering/TestReferenceImages.h, for pictures of the scene in different places drawn by the
	 * same machine, which differ only at the soft edges of shadows.
	 */
	constexpr int32 SameMachineChannelTolerance = 6;
	constexpr double SameMachineMaxDifferentPixelFraction = 0.002;

	/** Draws the test scene built around Origin. */
	FImage DrawTestScene(const FVector& Origin, FSceneRenderStatistics* OutStatistics = nullptr)
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		BuildRendererTestScene(*Test.Scene.Get(), Origin);

		const FImage Image =
			Test.Render(MakeRendererTestSceneView(Origin), RendererTestSceneWidth, RendererTestSceneHeight);
		if (OutStatistics != nullptr)
		{
			*OutStatistics = Test.Renderer->GetStatistics();
		}
		CHECK(CountLoggedErrors(Test.Capture) == 0);
		return Image;
	}

	/** Checks a picture against one of the renderer's reference images; see CheckAgainstReferenceImage. */
	void CheckAgainstReference(const FImage& Actual, const std::string& Name, const std::string& OutputSuffix = "")
	{
		CheckAgainstReferenceImage(Actual,
			FFileHelper::PathFromUtf8(WW_REFERENCE_IMAGE_DIRECTORY),
			FFileHelper::PathFromUtf8(WW_TEST_OUTPUT_DIRECTORY),
			Name,
			OutputSuffix);
	}
}

TEST_SUITE("Renderer test scene")
{
	TEST_CASE("The test scene is drawn as its reference image shows it")
	{
		FSceneRenderStatistics Statistics;

		const FImage Image = DrawTestScene(FVector(0.0), &Statistics);

		CheckAgainstReference(Image, "RendererTestScene");

		// The scene uses what it is meant to test: several meshes and textures, a lamp, a sun with its four shadow
		// maps, and a sky light with ambient occlusion.
		CHECK(Statistics.DrawnMeshCount == 7);
		CHECK(Statistics.CachedMeshCount == 4);
		CHECK(Statistics.CachedTextureCount == 3);
		CHECK(Statistics.DirectionalLightCount == 1);
		CHECK(Statistics.PointLightCount == 1);
		CHECK(Statistics.ShadowMapCount == 4);
		CHECK(Statistics.CachedEnvironmentCount == 1);
		CHECK(Statistics.SkyLightCount == 1);
		CHECK(Statistics.AmbientOcclusionPassCount == 1);
	}

	TEST_CASE("The test scene is drawn as its reference image shows it after it has played for a second")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		BuildRendererTestScene(*Test.Scene.Get(), FVector(0.0));
		Test.Scene->BeginPlay();
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Test.Scene->StepFrame();
		}

		const FImage Image =
			Test.Render(MakeRendererTestSceneView(FVector(0.0)), RendererTestSceneWidth, RendererTestSceneHeight);
		Test.Scene->EndPlay();

		CHECK(Test.Scene->GetPlayedSeconds() == 1.0);
		CheckAgainstReference(Image, "RendererTestScene", "AfterPlay");
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The reference image would not match the test scene without its sky light, shadows or ambient occlusion")
	{
		// A reference image tests only what its picture depends on. The scene is drawn whole and then with one of
		// the three switched off, and more of the picture has to change than the comparison with the reference
		// image lets pass: were one of them to stop working, the reference image would show it.
		FSceneRenderTest Test(ESceneViewMode::Lit);
		BuildRendererTestScene(*Test.Scene.Get(), FVector(0.0));
		const FCameraView View = MakeRendererTestSceneView(FVector(0.0));
		const FImage Whole = Test.Render(View, RendererTestSceneWidth, RendererTestSceneHeight);

		FImage Without;
		SUBCASE("Without the sky light")
		{
			const std::vector<WSkyLightComponent*> SkyLights = Test.Scene->GetComponents<WSkyLightComponent>();
			REQUIRE(SkyLights.size() == 1);
			SkyLights.front()->bVisible = false;

			Without = Test.Render(View, RendererTestSceneWidth, RendererTestSceneHeight);

			CHECK(Test.Renderer->GetStatistics().SkyLightCount == 0);
		}
		SUBCASE("Without shadows")
		{
			const std::vector<WDirectionalLightComponent*> Suns =
				Test.Scene->GetComponents<WDirectionalLightComponent>();
			REQUIRE(Suns.size() == 1);
			Suns.front()->bCastShadows = false;

			Without = Test.Render(View, RendererTestSceneWidth, RendererTestSceneHeight);

			CHECK(Test.Renderer->GetStatistics().ShadowMapCount == 0);
		}
		SUBCASE("Without ambient occlusion")
		{
			Test.Renderer->AmbientOcclusionIntensity = 0.0f;

			Without = Test.Render(View, RendererTestSceneWidth, RendererTestSceneHeight);

			CHECK(Test.Renderer->GetStatistics().AmbientOcclusionPassCount == 0);
		}

		const FImageDifference Difference = FImageDifference::Measure(Without, Whole, ReferenceChannelTolerance);
		INFO("Difference from the whole picture: ", Difference.Describe());
		CHECK_FALSE(Difference.IsWithin(ReferenceMaxDifferentPixelFraction));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The picture of the test scene is not empty or flat")
	{
		const FImage Image = DrawTestScene(FVector(0.0));

		// Many different colours, and the middle of the picture, where the gold sphere is, is not background.
		std::vector<FColor> DistinctColors;
		for (const FColor& Pixel : Image.GetPixels())
		{
			if (DistinctColors.size() < 1000 && std::ranges::find(DistinctColors, Pixel) == DistinctColors.end())
			{
				DistinctColors.push_back(Pixel);
			}
		}
		CHECK(DistinctColors.size() >= 1000);

		const FColor Background = Image.GetPixel(2, 2);
		CHECK(Image.GetPixel(RendererTestSceneWidth / 2, RendererTestSceneHeight / 2) != Background);
	}

	TEST_CASE("The test scene looks the same a thousand and ten billion kilometres from the world origin")
	{
		struct FCase
		{
			double Distance;
			const char* OutputSuffix;
		};

		const FImage AtOrigin = DrawTestScene(FVector(0.0));

		for (const FCase& Case : {FCase{1.0e6, "At1000Km"}, FCase{1.0e13, "At10BillionKm"}})
		{
			const FImage FarAway = DrawTestScene(FVector(Case.Distance, -Case.Distance, Case.Distance));

			CAPTURE(Case.Distance);
			// It matches the same reference image as the scene at the origin does.
			CheckAgainstReference(FarAway, "RendererTestScene", Case.OutputSuffix);
			// More than that, it is the picture the same machine drew at the origin, because everything is drawn
			// relative to the camera. Only the soft edges of shadows may differ by a shade: a shadow map is laid
			// out on a grid that is fixed in the world, and the scene sits on that grid differently in each place.
			const FImageDifference Difference =
				FImageDifference::Measure(FarAway, AtOrigin, SameMachineChannelTolerance);
			INFO("Difference from the picture at the origin: ", Difference.Describe());
			CHECK(Difference.IsWithin(SameMachineMaxDifferentPixelFraction));
		}
	}
}
