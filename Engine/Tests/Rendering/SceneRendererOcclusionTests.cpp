#include "Rendering/EnvironmentMap.h"
#include "Rendering/HdrImage.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SkyLightComponent.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneLighting.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <ostream>
#include <string>
#include <vector>

// Most of these tests use the view mode that shows ambient occlusion by itself: white, 255, where a surface is
// in the open, and darker where it is hidden from its surroundings.
//
// The corner they look at is a floor one metre below the camera and a wall three metres ahead of it, which meet
// in a crease. In a picture 128 pixels high taken with the wide test camera, the crease is in row 85: the wall is
// above it and the floor below.

namespace
{
	constexpr int32 Size = 128;
	constexpr int32 CreaseRow = 85;

	/** A row of the wall and a row of the floor that are much farther from the crease than occlusion reaches. */
	constexpr int32 OpenWallRow = 30;
	constexpr int32 OpenFloorRow = 122;

	const FQuat FacingUp = Turn(-90.0f, AxisX);

	/** A large floor one metre below the camera. */
	void AddFloor(WScene& Scene, const FVector& Origin = FVector(0.0))
	{
		AddSquare(Scene, MakeTransform(Origin + FVector(0.0, -1.0, 0.0), FacingUp, FVector3f(40.0f)), LinearRed);
	}

	/** A wall that faces the camera from three metres ahead and stands on the floor. */
	void AddWall(WScene& Scene, const FVector& Origin = FVector(0.0))
	{
		AddSquare(Scene,
			MakeTransform(Origin + FVector(0.0, 9.0, -3.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(20.0f)),
			LinearGreen);
	}

	FImage RenderOcclusion(FSceneRenderTest& Test, const FCameraView& View = MakeWideView(), int32 SideLength = Size)
	{
		Test.Renderer->ViewMode = ESceneViewMode::AmbientOcclusion;
		return Test.Render(View, SideLength, SideLength);
	}

	/** The darkest shade of grey in a part of a row. */
	int32 GetDarkest(const FImage& Image, int32 Row, int32 FirstColumn = 16, int32 LastColumn = Size - 17)
	{
		int32 Darkest = 255;
		for (int32 X = FirstColumn; X <= LastColumn; ++X)
		{
			Darkest = std::min(Darkest, static_cast<int32>(Image.GetPixel(X, Row).R));
		}
		return Darkest;
	}

	/** The largest number of shades by which a pixel differs from the next one in its row, anywhere in the picture. */
	int32 GetLargestStepAlongRows(const FImage& Image, int32 FirstColumn = 16, int32 LastColumn = Size - 17)
	{
		int32 LargestStep = 0;
		for (int32 Y = 0; Y < Image.GetHeight(); ++Y)
		{
			for (int32 X = FirstColumn; X < LastColumn; ++X)
			{
				const int32 Shade = Image.GetPixel(X, Y).R;
				const int32 NextShade = Image.GetPixel(X + 1, Y).R;
				LargestStep = std::max(LargestStep, std::abs(Shade - NextShade));
			}
		}
		return LargestStep;
	}

	/**
	 * The largest number of shades by which a column gets darker again on the way from its darkest pixel to the
	 * top or to the bottom of the picture: zero if the occlusion of a crease fades away evenly on both sides.
	 */
	int32 GetLargestRipple(const FImage& Image, int32 Column)
	{
		int32 DarkestRow = 0;
		for (int32 Y = 1; Y < Image.GetHeight(); ++Y)
		{
			if (Image.GetPixel(Column, Y).R < Image.GetPixel(Column, DarkestRow).R)
			{
				DarkestRow = Y;
			}
		}

		int32 LargestRipple = 0;
		for (int32 Y = DarkestRow; Y > 0; --Y)
		{
			LargestRipple = std::max(LargestRipple, Image.GetPixel(Column, Y).R - Image.GetPixel(Column, Y - 1).R);
		}
		for (int32 Y = DarkestRow; Y < Image.GetHeight() - 1; ++Y)
		{
			LargestRipple = std::max(LargestRipple, Image.GetPixel(Column, Y).R - Image.GetPixel(Column, Y + 1).R);
		}
		return LargestRipple;
	}

	int32 CountPixelsThatAreNot(const FImage& Image, FColor Color)
	{
		int32 Count = 0;
		for (const FColor& Pixel : Image.GetPixels())
		{
			Count += Pixel == Color ? 0 : 1;
		}
		return Count;
	}
}

TEST_SUITE("FSceneRenderer ambient occlusion")
{
	TEST_CASE("Nothing is occluded in an empty scene")
	{
		FSceneRenderTest Test;

		const FImage Image = RenderOcclusion(Test);

		CHECK(Image == FImage(Size, Size, White));
		CHECK(Test.Renderer->GetStatistics().AmbientOcclusionPassCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A flat surface in the open is not occluded, whether it is seen head-on or at a flat angle")
	{
		{
			FSceneRenderTest Test;
			AddSquare(*Test.Scene.Get(),
				MakeTransform(FVector(0.0, 0.0, -4.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(12.0f)),
				LinearRed);

			CHECK(CountPixelsThatAreNot(RenderOcclusion(Test), White) == 0);
		}

		{
			// A floor that stretches from under the camera to the horizon, with the sky above it.
			FSceneRenderTest Test;
			AddFloor(*Test.Scene.Get());

			CHECK(CountPixelsThatAreNot(RenderOcclusion(Test), White) == 0);
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}

		{
			// The same floor seen from close above, looking steeply down, and from a tilted camera.
			FSceneRenderTest Test;
			AddFloor(*Test.Scene.Get());
			FCameraView View = MakeWideView(FVector(0.0, -0.7, 0.0));
			View.Rotation = Turn(25.0f, AxisY) * Turn(-65.0f, AxisX);

			CHECK(CountPixelsThatAreNot(RenderOcclusion(Test, View), White) == 0);
		}
	}

	TEST_CASE("The inside of a corner is darker towards the crease, and not at all far from it")
	{
		FSceneRenderTest Test;
		AddFloor(*Test.Scene.Get());
		AddWall(*Test.Scene.Get());

		const FImage Image = RenderOcclusion(Test);

		// Dark at the crease, on the wall just above it and on the floor just below it.
		CHECK(GetDarkest(Image, CreaseRow) < 190);
		CHECK(Image.GetPixel(64, CreaseRow - 2).R < 225);
		CHECK(Image.GetPixel(64, CreaseRow + 1).R < 225);
		// Lighter away from it, on both sides. The floor, seen at a flat angle, takes up few rows of the picture.
		CHECK(Image.GetPixel(64, CreaseRow - 2).R < Image.GetPixel(64, CreaseRow - 6).R);
		CHECK(Image.GetPixel(64, CreaseRow + 1).R < Image.GetPixel(64, CreaseRow + 4).R);
		// Untouched where the other surface is more than the radius away.
		for (int32 X = 0; X < Size; X += 7)
		{
			CHECK(Image.GetPixel(X, OpenWallRow) == White);
			CHECK(Image.GetPixel(X, OpenFloorRow) == White);
		}
		// The crease is about as dark at the sides of the picture as in its middle.
		const int32 Darkest = GetDarkest(Image, CreaseRow);
		for (int32 X = 16; X < Size - 16; X += 5)
		{
			CHECK(Image.GetPixel(X, CreaseRow).R <= Darkest + 30);
		}
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Occlusion is smooth: no grain along a crease and no ripples on the way from it")
	{
		// Every pixel takes its samples at one of sixteen angles, in a pattern that repeats every four pixels.
		// The smoothing afterwards has to cancel that pattern, not just soften it into a grain. A crease that runs
		// straight across the picture shows whether it does: all pixels of a row have the same surroundings, so
		// they must be equally dark, and above and below the crease the picture must only get lighter.
		FSceneRenderTest Test;
		AddFloor(*Test.Scene.Get());
		AddWall(*Test.Scene.Get());

		FCameraView View;
		SUBCASE("Seen level")
		{
			View = MakeWideView();
		}
		SUBCASE("Seen from above, looking down")
		{
			View = MakeWideView(FVector(0.0, 0.5, 0.0));
			View.Rotation = Turn(-30.0f, AxisX);
		}

		const FImage Image = RenderOcclusion(Test, View);

		// The crease is in the picture, and it is dark.
		int32 Darkest = 255;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			Darkest = std::min(Darkest, GetDarkest(Image, Y));
		}
		CHECK(Darkest < 190);

		// A pixel differs from the next in its row by a shade or two of 255, because the crease is not quite as
		// dark at the sides of the picture as in its middle. Grain would be ten times that.
		CHECK(GetLargestStepAlongRows(Image) <= 4);

		int32 LargestRipple = 0;
		for (int32 X = 16; X < Size - 16; ++X)
		{
			LargestRipple = std::max(LargestRipple, GetLargestRipple(Image, X));
		}
		CHECK(LargestRipple <= 2);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The radius says how far occlusion reaches, in metres")
	{
		FSceneRenderTest Test;
		AddFloor(*Test.Scene.Get());
		AddWall(*Test.Scene.Get());
		// A row of the wall half a metre above the crease, where a radius of half a metre just ends.
		const int32 Row = CreaseRow - 10;

		Test.Renderer->AmbientOcclusionRadius = 0.5f;
		CHECK(GetDarkest(RenderOcclusion(Test), Row) >= 250);

		Test.Renderer->AmbientOcclusionRadius = 2.5f;
		const FImage Wide = RenderOcclusion(Test);
		CHECK(GetDarkest(Wide, Row) < 240);
		// And a metre and a half above the crease it has faded out again.
		CHECK(GetDarkest(Wide, CreaseRow - 45) >= 250);
	}

	TEST_CASE("The intensity says how dark occlusion is, and zero switches it off")
	{
		FSceneRenderTest Test;
		AddFloor(*Test.Scene.Get());
		AddWall(*Test.Scene.Get());

		Test.Renderer->AmbientOcclusionIntensity = 1.0f;
		const int32 Natural = GetDarkest(RenderOcclusion(Test), CreaseRow);
		Test.Renderer->AmbientOcclusionIntensity = 0.3f;
		const int32 Faint = GetDarkest(RenderOcclusion(Test), CreaseRow);
		Test.Renderer->AmbientOcclusionIntensity = 3.0f;
		const int32 Strong = GetDarkest(RenderOcclusion(Test), CreaseRow);

		CHECK(Faint > Natural + 10);
		CHECK(Strong < Natural - 10);

		for (const float Intensity : {0.0f, -1.0f})
		{
			Test.Renderer->AmbientOcclusionIntensity = Intensity;
			CHECK(RenderOcclusion(Test) == FImage(Size, Size, White));
			CHECK(Test.Renderer->GetStatistics().AmbientOcclusionPassCount == 0);
		}

		Test.Renderer->AmbientOcclusionIntensity = 1.0f;
		Test.Renderer->AmbientOcclusionRadius = 0.0f;
		CHECK(RenderOcclusion(Test) == FImage(Size, Size, White));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Something near the camera does not darken what is far behind it")
	{
		FSceneRenderTest Test;
		// A wall six metres away, and a small square one metre away in front of its middle.
		AddSquare(*Test.Scene.Get(),
			MakeTransform(FVector(0.0, 0.0, -6.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(20.0f)),
			LinearRed);
		AddSquare(*Test.Scene.Get(),
			MakeTransform(FVector(0.0, 0.0, -1.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(0.5f)),
			LinearGreen);

		const FImage Image = RenderOcclusion(Test);

		// Neither the square, nor the wall around its outline, is occluded: they are five metres apart.
		CHECK(CountPixelsThatAreNot(Image, White) == 0);
	}

	TEST_CASE("A thing that stands on a floor darkens the floor around its foot")
	{
		FSceneRenderTest Test;
		AddFloor(*Test.Scene.Get());
		WEntity* const Box = Test.Scene->CreateEntity("Box");
		Box->SetRelativeTransform(MakeTransform(FVector(0.0, -0.5, -3.0)));
		Box->AddComponent<WStaticMeshComponent>()->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		// From a little above, looking down at the box.
		FCameraView View = MakeWideView(FVector(0.0, 0.5, 0.0));
		View.Rotation = Turn(-25.0f, AxisX);

		const FImage Image = RenderOcclusion(Test, View);

		// Find the floor just in front of the box in the middle column: going down the picture from the box's
		// front face, the first rows of floor are dark and the rows farther down are white again.
		int32 DarkestRow = 0;
		int32 Darkest = 255;
		for (int32 Y = Size / 2; Y < Size; ++Y)
		{
			if (Image.GetPixel(64, Y).R < Darkest)
			{
				Darkest = Image.GetPixel(64, Y).R;
				DarkestRow = Y;
			}
		}
		CHECK(Darkest < 200);
		CHECK(DarkestRow > Size / 2);
		CHECK(Image.GetPixel(64, Size - 3) == White);
		// The top of the box, in the open, is not occluded, and neither is the floor far to the side.
		CHECK(Image.GetPixel(4, Size - 3) == White);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Occlusion is measured in the world, so it looks the same in a larger picture")
	{
		FSceneRenderTest Test;
		AddFloor(*Test.Scene.Get());
		AddWall(*Test.Scene.Get());

		const FImage Small = RenderOcclusion(Test);
		const FImage Large = RenderOcclusion(Test, MakeWideView(), Size * 2);

		// The band of wall above the crease is as dark, and the open parts as white, at twice the size. A single
		// row cannot be compared: the crease does not fall on the same part of a pixel in both pictures.
		const auto GetBandMean = [](const FImage& Image, int32 Column, int32 FirstRow, int32 LastRow)
		{
			int32 Sum = 0;
			for (int32 Y = FirstRow; Y <= LastRow; ++Y)
			{
				Sum += Image.GetPixel(Column, Y).R;
			}
			return Sum / (LastRow - FirstRow + 1);
		};
		const int32 SmallBand = GetBandMean(Small, 64, CreaseRow - 7, CreaseRow - 1);
		const int32 LargeBand = GetBandMean(Large, 128, CreaseRow * 2 - 14, CreaseRow * 2 - 1);
		CHECK(SmallBand < 235);
		CHECK(std::abs(SmallBand - LargeBand) < 12);
		CHECK(Large.GetPixel(128, OpenWallRow * 2) == White);
		CHECK(Large.GetPixel(128, OpenFloorRow * 2) == White);
	}

	TEST_CASE("Ambient occlusion dims the light of the sky and leaves the light of a sun alone")
	{
		// The corner, drawn lit, with ambient occlusion and without.
		const auto RenderLit = [](bool bWithSky, bool bWithSun, float Radius)
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			AddFloor(*Test.Scene.Get());
			AddWall(*Test.Scene.Get());
			if (bWithSky)
			{
				WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
				std::string Error;
				REQUIRE(Environment->SetImage(FHdrImage(32, 16, FLinearColor(1.0f, 1.0f, 1.0f)), Error));
				Test.Scene->CreateEntity("Sky")->AddComponent<WSkyLightComponent>()->Environment = Environment;
			}
			if (bWithSun)
			{
				// From behind and above the camera, without shadows, which are another matter.
				AddSun(*Test.Scene.Get(), Turn(-40.0f, AxisX))->bCastShadows = false;
			}
			Test.Renderer->AmbientOcclusionRadius = Radius;
			const FImage Image = Test.Render(MakeWideView(), Size, Size);
			CHECK(CountLoggedErrors(Test.Capture) == 0);
			return Image;
		};

		// Under the sky alone, the crease is darker with ambient occlusion than without, and the open floor is not.
		const FImage SkyWith = RenderLit(true, false, 0.5f);
		const FImage SkyWithout = RenderLit(true, false, 0.0f);
		CHECK(SkyWith.GetPixel(64, CreaseRow + 1).R < SkyWithout.GetPixel(64, CreaseRow + 1).R - 15);
		CHECK(SkyWith.GetPixel(64, CreaseRow - 2).G < SkyWithout.GetPixel(64, CreaseRow - 2).G - 10);
		CHECK(SkyWith.GetPixel(64, OpenFloorRow) == SkyWithout.GetPixel(64, OpenFloorRow));
		CHECK(SkyWith.GetPixel(64, OpenWallRow) == SkyWithout.GetPixel(64, OpenWallRow));

		// Under a sun alone, ambient occlusion changes nothing at all.
		CHECK(RenderLit(false, true, 0.5f) == RenderLit(false, true, 0.0f));
	}

	TEST_CASE("Ambient occlusion is the same a thousand and ten billion kilometres from the world origin")
	{
		std::vector<FImage> Images;
		for (const double Distance : {0.0, 1.0e6, 1.0e13})
		{
			FSceneRenderTest Test;
			const FVector Origin(Distance, Distance, -Distance);
			AddFloor(*Test.Scene.Get(), Origin);
			AddWall(*Test.Scene.Get(), Origin);

			Images.push_back(RenderOcclusion(Test, MakeWideView(Origin)));
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}

		CHECK(GetDarkest(Images[0], CreaseRow) < 190);
		CHECK(Images[1] == Images[0]);
		CHECK(Images[2] == Images[0]);
	}
}
