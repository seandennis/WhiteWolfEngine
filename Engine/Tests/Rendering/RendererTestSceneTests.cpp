#include "Core/FileHelper.h"
#include "Rendering/Image.h"
#include "Rendering/ImageDifference.h"
#include "Rendering/ImageFile.h"
#include "Rendering/RendererTestScene.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// The renderer's test scene is drawn without a window and compared with a reference image that is stored in the
// repository. Two graphics cards never draw exactly the same picture, so the comparison allows a small difference;
// see docs/DECISIONS.md, D-034, for the tolerances and for how a reference image is replaced.

namespace
{
	/** By how many steps of 255 a channel may differ from the reference before its pixel counts as different. */
	constexpr int32 ChannelTolerance = 12;

	/** The fraction of the pixels that may differ from the reference by more than the tolerance. */
	constexpr double MaxDifferentPixelFraction = 0.02;

	/**
	 * The same two limits for pictures of the scene in different places drawn by the same machine, which differ
	 * only at the soft edges of shadows.
	 */
	constexpr int32 SameMachineChannelTolerance = 6;
	constexpr double SameMachineMaxDifferentPixelFraction = 0.002;

	std::filesystem::path PathFromUtf8(std::string_view Text)
	{
		return std::filesystem::path(std::u8string(Text.begin(), Text.end()));
	}

	/** Draws the test scene built around Origin. */
	FImage DrawTestScene(const FVector& Origin, FSceneRenderStatistics* OutStatistics = nullptr)
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		Test.Renderer->BackgroundColor = RendererTestSceneBackground;
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

	/**
	 * Checks a picture against a reference image. If it does not match, or there is no reference image yet, the
	 * picture is saved in the build directory, where it can be looked at and, if it is right, copied over the
	 * reference image.
	 */
	void CheckAgainstReference(const FImage& Actual, const std::string& Name)
	{
		const std::filesystem::path ReferencePath = PathFromUtf8(WW_REFERENCE_IMAGE_DIRECTORY) / (Name + ".png");
		const std::filesystem::path OutputPath = PathFromUtf8(WW_TEST_OUTPUT_DIRECTORY) / (Name + ".png");

		FImage Reference;
		std::string Error;
		const bool bHasReference = FImageFile::Load(ReferencePath, Reference, Error);
		const FImageDifference Difference = FImageDifference::Measure(Actual, Reference, ChannelTolerance);
		const bool bMatches = bHasReference && Difference.IsWithin(MaxDifferentPixelFraction);
		if (!bMatches)
		{
			std::string SaveError;
			const bool bSaved = FImageFile::SavePng(Actual, OutputPath, SaveError);
			INFO("Reference image: ", FFileHelper::PathToUtf8(ReferencePath));
			INFO("What was drawn: ", bSaved ? FFileHelper::PathToUtf8(OutputPath) : SaveError);
			INFO("Difference: ", bHasReference ? Difference.Describe() : Error);
			CHECK(bMatches);
		}
	}
}

TEST_SUITE("Renderer test scene")
{
	TEST_CASE("The test scene is drawn as its reference image shows it")
	{
		FSceneRenderStatistics Statistics;

		const FImage Image = DrawTestScene(FVector(0.0), &Statistics);

		CheckAgainstReference(Image, "RendererTestScene");

		// The scene uses what it is meant to test: several meshes and textures and both kinds of light.
		CHECK(Statistics.DrawnMeshCount == 7);
		CHECK(Statistics.CachedMeshCount == 4);
		CHECK(Statistics.CachedTextureCount == 3);
		CHECK(Statistics.DirectionalLightCount == 1);
		CHECK(Statistics.PointLightCount == 1);
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
		const FImage AtOrigin = DrawTestScene(FVector(0.0));

		for (const double Distance : {1.0e6, 1.0e13})
		{
			const FImage FarAway = DrawTestScene(FVector(Distance, -Distance, Distance));

			CAPTURE(Distance);
			// It matches the same reference image as the scene at the origin does.
			CheckAgainstReference(FarAway, "RendererTestScene");
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
