#include "Rendering/MipChain.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneLighting.h"
#include "Rendering/TestSceneRendering.h"
#include "Rendering/Texture.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cstdlib>
#include <memory>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

// In the unlit tests a square of one metre, two metres ahead of the wide test camera, covers exactly 16 by 16
// pixels of a 64 pixel picture: pixels 24 to 39. A texture of 16 by 16 pixels on it is therefore shown pixel
// for pixel, so every pixel of the picture has exactly the colour of one pixel of the texture.

namespace
{
	constexpr int32 SquareFirstPixel = 24;
	constexpr int32 SquarePixelCount = 16;

	WTexture* MakeTexture(const FImage& Image,
		ETextureColorSpace ColorSpace = ETextureColorSpace::Srgb,
		ETextureWrap Wrap = ETextureWrap::Repeat)
	{
		WTexture* const Texture = NewObject<WTexture>();
		std::string Error;
		REQUIRE(Texture->SetImage(Image, ColorSpace, Error));
		Texture->WrapU = Wrap;
		Texture->WrapV = Wrap;
		return Texture;
	}

	/** A square picture whose four quarters have one colour each. */
	FImage MakeQuarters(int32 Size, FColor TopLeft, FColor TopRight, FColor BottomLeft, FColor BottomRight)
	{
		FImage Image(Size, Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const bool bIsLeft = X < Size / 2;
				const bool bIsTop = Y < Size / 2;
				Image.SetPixel(X, Y, bIsTop ? (bIsLeft ? TopLeft : TopRight) : (bIsLeft ? BottomLeft : BottomRight));
			}
		}
		return Image;
	}

	/** Adds the white test square with a base colour texture. */
	WStaticMeshComponent* AddTexturedSquare(WScene& Scene, WTexture* Texture)
	{
		WStaticMeshComponent* const Component =
			AddSquare(Scene, MakeTransform(FVector(0.0, 0.0, -2.0)), FLinearColor(1.0f, 1.0f, 1.0f));
		Component->Material->BaseColorTexture = Texture;
		return Component;
	}

	/** Returns the part of a picture that shows the test square, which is the texture as it was drawn. */
	FImage CutOutSquare(const FImage& Picture)
	{
		FImage Square(SquarePixelCount, SquarePixelCount);
		for (int32 Y = 0; Y < SquarePixelCount; ++Y)
		{
			for (int32 X = 0; X < SquarePixelCount; ++X)
			{
				Square.SetPixel(X, Y, Picture.GetPixel(SquareFirstPixel + X, SquareFirstPixel + Y));
			}
		}
		return Square;
	}

	bool IsWithin(FColor Actual, FColor Expected, int32 Tolerance)
	{
		return std::abs(Actual.R - Expected.R) <= Tolerance && std::abs(Actual.G - Expected.G) <= Tolerance &&
			std::abs(Actual.B - Expected.B) <= Tolerance && std::abs(Actual.A - Expected.A) <= Tolerance;
	}

	/**
	 * Replaces the texture coordinates of the test square. The corners are given in the order bottom left, bottom
	 * right, top right, top left, as seen from the front.
	 */
	void SetTextureCoordinates(WStaticMeshComponent& Component,
		const FVector2f& BottomLeft,
		const FVector2f& BottomRight,
		const FVector2f& TopRight,
		const FVector2f& TopLeft)
	{
		const std::vector<FStaticMeshVertex> Vertices{
			{FVector3f(-0.5f, -0.5f, 0.0f), AxisZ, BottomLeft},
			{FVector3f(0.5f, -0.5f, 0.0f), AxisZ, BottomRight},
			{FVector3f(0.5f, 0.5f, 0.0f), AxisZ, TopRight},
			{FVector3f(-0.5f, 0.5f, 0.0f), AxisZ, TopLeft},
		};
		std::string Error;
		REQUIRE(Component.Mesh->SetGeometry(Vertices, {0, 1, 2, 0, 2, 3}, Error));
	}

	/** The direction a normal texture's pixel stands for, relative to the surface. */
	FVector DecodeNormal(FColor Pixel, double Scale = 1.0)
	{
		const FVector Direction(Pixel.R / 255.0 * 2.0 - 1.0, Pixel.G / 255.0 * 2.0 - 1.0, Pixel.B / 255.0 * 2.0 - 1.0);
		return glm::normalize(FVector(Direction.x * Scale, Direction.y * Scale, Direction.z));
	}

	/** A normal texture pixel that leans the surface towards the picture's right, and one that leans it upwards. */
	constexpr FColor LeanRight(204, 128, 230);
	constexpr FColor LeanUp(128, 204, 230);
}

TEST_SUITE("FSceneRenderer textures")
{
	TEST_CASE("A base colour texture lies on a surface like a picture on a wall")
	{
		FSceneRenderTest Test;
		const FImage Picture = MakeQuarters(16, Red, Green, Blue, White);
		AddTexturedSquare(*Test.Scene.Get(), MakeTexture(Picture));

		const FImage Image = Test.Render(MakeWideView());

		CHECK(CutOutSquare(Image) == Picture);
		CHECK(Image.GetPixel(23, 23) == Magenta);
		CHECK(Image.GetPixel(40, 40) == Magenta);
		CHECK(Test.Renderer->GetStatistics().UploadedTextureCount == 1);
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Every pixel of a texture arrives where it belongs")
	{
		FSceneRenderTest Test;
		// A picture in which no two pixels have the same colour.
		FImage Picture(16, 16);
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				Picture.SetPixel(X, Y, FColor(static_cast<uint8>(X * 17), static_cast<uint8>(Y * 17), 255));
			}
		}
		AddTexturedSquare(*Test.Scene.Get(), MakeTexture(Picture, ETextureColorSpace::Srgb, ETextureWrap::ClampToEdge));

		const FImage Square = CutOutSquare(Test.Render(MakeWideView()));

		int32 WrongPixelCount = 0;
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				WrongPixelCount += IsWithin(Square.GetPixel(X, Y), Picture.GetPixel(X, Y), 1) ? 0 : 1;
			}
		}
		CHECK(WrongPixelCount == 0);
	}

	TEST_CASE("A texture multiplies the material's base colour")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddTexturedSquare(*Test.Scene.Get(), MakeTexture(MakeQuarters(16, Red, Green, Blue, White)));
		Component->Material->BaseColor = FLinearColor(1.0f, 0.0f, 1.0f);

		const FImage Image = Test.Render(MakeWideView());

		// Only what is in both the colour and the texture is left.
		CHECK(CutOutSquare(Image) == MakeQuarters(16, Red, Black, Blue, Magenta));
	}

	TEST_CASE("Outside its picture a texture repeats, mirrors or continues its edge, as it is told")
	{
		// Half red, half green, on a square whose texture coordinates run from 0 to 2 in the tested direction:
		// the picture is seen twice, each of its pixels on one pixel.
		FImage LeftRight(8, 8, Red);
		FImage TopBottom(8, 8, Red);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			for (int32 Other = 4; Other < 8; ++Other)
			{
				LeftRight.SetPixel(Other, Index, Green);
				TopBottom.SetPixel(Index, Other, Green);
			}
		}

		struct FCase
		{
			ETextureWrap Wrap;
			// The colours of the four stripes that the 16 pixels across the square fall into.
			FColor Stripes[4];
		};

		for (const FCase& Case : {
				 FCase{ETextureWrap::Repeat, {Red, Green, Red, Green}},
				 FCase{ETextureWrap::MirroredRepeat, {Red, Green, Green, Red}},
				 FCase{ETextureWrap::ClampToEdge, {Red, Green, Green, Green}},
			 })
		{
			FSceneRenderTest Test;
			WTexture* const Across = MakeTexture(LeftRight);
			Across->WrapU = Case.Wrap;
			WStaticMeshComponent* const Component = AddTexturedSquare(*Test.Scene.Get(), Across);
			SetTextureCoordinates(
				*Component, FVector2f(0.0f, 1.0f), FVector2f(2.0f, 1.0f), FVector2f(2.0f, 0.0f), FVector2f(0.0f, 0.0f));
			const FImage Horizontal = CutOutSquare(Test.Render(MakeWideView()));

			WTexture* const Down = MakeTexture(TopBottom);
			Down->WrapV = Case.Wrap;
			Component->Material->BaseColorTexture = Down;
			SetTextureCoordinates(
				*Component, FVector2f(0.0f, 2.0f), FVector2f(1.0f, 2.0f), FVector2f(1.0f, 0.0f), FVector2f(0.0f, 0.0f));
			const FImage Vertical = CutOutSquare(Test.Render(MakeWideView()));

			for (int32 Along = 0; Along < 16; ++Along)
			{
				for (int32 Offset = 0; Offset < 16; ++Offset)
				{
					CAPTURE(Along);
					CAPTURE(Offset);
					REQUIRE(Horizontal.GetPixel(Along, Offset) == Case.Stripes[Along / 4]);
					REQUIRE(Vertical.GetPixel(Offset, Along) == Case.Stripes[Along / 4]);
				}
			}
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("An sRGB texture holds colours and a linear texture holds numbers")
	{
		const FImage Grey(16, 16, FColor(128, 128, 128));

		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddTexturedSquare(*Test.Scene.Get(), MakeTexture(Grey, ETextureColorSpace::Srgb));

		// An sRGB colour is decoded when it is read and encoded again when it is drawn: it comes out as it went in.
		CHECK(IsWithin(Test.Render(MakeWideView()).GetPixel(32, 32), FColor(128, 128, 128), 1));

		// A linear 128 is the number 0.5, which is 188 once drawn as a colour.
		Component->Material->BaseColorTexture = MakeTexture(Grey, ETextureColorSpace::Linear);
		CHECK(IsWithin(Test.Render(MakeWideView()).GetPixel(32, 32), FColor(188, 188, 188), 1));
	}

	TEST_CASE("A texture seen from a distance shows the average of its pixels, not a random few of them")
	{
		FSceneRenderTest Test;
		// A checkerboard of single black and white pixels, 64 pixels a side, on a square that is 16 pixels a side
		// in the picture. Each pixel of the picture covers 16 pixels of the texture, half of them white.
		FImage Checkerboard(64, 64);
		for (int32 Y = 0; Y < 64; ++Y)
		{
			for (int32 X = 0; X < 64; ++X)
			{
				Checkerboard.SetPixel(X, Y, (X + Y) % 2 == 0 ? White : Black);
			}
		}
		AddTexturedSquare(*Test.Scene.Get(), MakeTexture(Checkerboard));

		const FImage Square = CutOutSquare(Test.Render(MakeWideView()));

		// Half the light of white, which is 188 as an sRGB value.
		int32 WrongPixelCount = 0;
		for (const FColor& Pixel : Square.GetPixels())
		{
			WrongPixelCount += IsWithin(Pixel, FColor(188, 188, 188), 2) ? 0 : 1;
		}
		CHECK(WrongPixelCount == 0);
	}

	TEST_CASE("A texture seen from close up is smooth between its pixels and exact at their centres")
	{
		FSceneRenderTest Test;
		// Two by two pixels over 16 by 16 pixels of the picture.
		AddTexturedSquare(*Test.Scene.Get(),
			MakeTexture(MakeQuarters(2, Red, Green, Blue, White), ETextureColorSpace::Srgb, ETextureWrap::ClampToEdge));

		const FImage Square = CutOutSquare(Test.Render(MakeWideView()));

		// Towards the corners, beyond the centres of the texture's pixels, the colours are pure.
		CHECK(Square.GetPixel(1, 1) == Red);
		CHECK(Square.GetPixel(14, 1) == Green);
		CHECK(Square.GetPixel(1, 14) == Blue);
		CHECK(Square.GetPixel(14, 14) == White);
		// Half way between red and green there is some of both and no blue.
		const FColor Between = Square.GetPixel(7, 1);
		CHECK(Between.R > 100);
		CHECK(Between.G > 100);
		CHECK(Between.B == 0);
	}

	TEST_CASE("A texture is copied to the graphics card once, however many materials use it")
	{
		FSceneRenderTest Test;
		WTexture* const Texture = MakeTexture(FImage(16, 16, Green));
		AddTexturedSquare(*Test.Scene.Get(), Texture);
		WStaticMeshComponent* const Second = AddTexturedSquare(*Test.Scene.Get(), Texture);
		Second->GetOwner()->SetRelativeTransform(MakeTransform(FVector(1.0, 0.0, -2.0)));

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Green);
		CHECK(Image.GetPixel(44, 32) == Green);
		CHECK(Test.Renderer->GetStatistics().UploadedTextureCount == 1);
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 1);

		Test.Render(MakeWideView());
		CHECK(Test.Renderer->GetStatistics().UploadedTextureCount == 0);
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A new picture for a texture, of any size, shows in the next picture")
	{
		FSceneRenderTest Test;
		WTexture* const Texture = MakeTexture(FImage(16, 16, Green));
		AddTexturedSquare(*Test.Scene.Get(), Texture);
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Green);

		std::string Error;
		REQUIRE(Texture->SetImage(FImage(5, 3, Blue), ETextureColorSpace::Srgb, Error));

		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Blue);
		CHECK(Test.Renderer->GetStatistics().UploadedTextureCount == 1);
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Giving a material another texture, or its texture another wrap mode, shows in the next picture")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddTexturedSquare(*Test.Scene.Get(), MakeTexture(FImage(16, 16, Green)));
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Green);

		// Another texture.
		FImage Picture(8, 8, Red);
		Picture.SetPixel(7, 0, Blue);
		WTexture* const Texture = MakeTexture(Picture, ETextureColorSpace::Srgb, ETextureWrap::ClampToEdge);
		Component->Material->BaseColorTexture = Texture;
		SetTextureCoordinates(
			*Component, FVector2f(1.0f, 1.0f), FVector2f(3.0f, 1.0f), FVector2f(3.0f, -1.0f), FVector2f(1.0f, -1.0f));
		// The square now shows what lies beyond the picture's top right corner: with clamping, its corner pixel.
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Blue);

		// The same texture, repeating: beyond the corner the picture starts again.
		Texture->WrapU = ETextureWrap::Repeat;
		Texture->WrapV = ETextureWrap::Repeat;
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Red);

		// No texture at all.
		Component->Material->BaseColorTexture = nullptr;
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == White);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A texture that has no picture or has been destroyed counts as no texture")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component = AddTexturedSquare(*Test.Scene.Get(), NewObject<WTexture>());
		Component->Material->BaseColor = LinearGreen;
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Green);
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 0);

		WTexture* const Texture = MakeTexture(FImage(4, 4, Black));
		Component->Material->BaseColorTexture = Texture;
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Black);

		Texture->Destroy();
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Green);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The copy of a texture on the graphics card is released when the texture is collected")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddTexturedSquare(*Test.Scene.Get(), MakeTexture(FImage(16, 16, Green)));
		Test.Render(MakeWideView());
		REQUIRE(Test.Renderer->GetStatistics().CachedTextureCount == 1);

		// A texture that is no longer drawn keeps its copy for as long as it exists.
		Component->bVisible = false;
		Test.Render(MakeWideView());
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 1);

		Component->GetOwner()->Destroy();
		CollectGarbage();
		Test.Render(MakeWideView());

		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A metallic-roughness texture scales roughness with its green and metallic with its blue channel")
	{
		struct FCase
		{
			FColor Pixel;
			double Roughness;
			double Metallic;
		};

		for (const FCase& Case : {
				 FCase{FColor(0, 255, 255), 0.8, 1.0},
				 FCase{FColor(255, 128, 255), 0.8 * 128.0 / 255.0, 1.0},
				 FCase{FColor(255, 255, 0), 0.8, 0.0},
				 FCase{FColor(255, 160, 128), 0.8 * 160.0 / 255.0, 128.0 / 255.0},
			 })
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			FReferenceSurface Surface;
			Surface.BaseColor = FReferenceColor(0.9, 0.6, 0.3);
			Surface.Metallic = 1.0;
			Surface.Roughness = 0.8;
			WStaticMeshComponent* const Component = AddSurface(*Test.Scene.Get(), Surface);
			Component->Material->MetallicRoughnessTexture =
				MakeTexture(FImage(4, 4, Case.Pixel), ETextureColorSpace::Linear);
			const FQuat Rotation = Turn(25.0f, AxisY);
			AddSun(*Test.Scene.Get(), Rotation);

			const FImage Image = RenderPicture(Test);

			Surface.Roughness = Case.Roughness;
			Surface.Metallic = Case.Metallic;
			const FVector ToLight = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
			CAPTURE(Case.Roughness);
			CAPTURE(Case.Metallic);
			CheckPixel(Image,
				Middle,
				Middle,
				ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}
	}

	TEST_CASE("An emissive texture multiplies the material's emissive colour")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		Surface.EmissiveColor = FReferenceColor(0.5, 1.0, 2.0);
		WStaticMeshComponent* const Component = AddSurface(*Test.Scene.Get(), Surface);
		// White on the left half and black on the right half.
		Component->Material->EmissiveTexture = MakeTexture(MakeQuarters(16, White, Black, White, Black));

		const FImage Image = RenderPicture(Test);

		CheckPixel(Image, Middle - 4, Middle, ReferencePixel(Surface.EmissiveColor));
		CHECK(Image.GetPixel(Middle + 4, Middle) == Black);

		// Without an emissive colour the texture adds nothing.
		Component->Material->EmissiveColor = LinearBlack;
		CHECK(RenderPicture(Test).GetPixel(Middle - 4, Middle) == Black);
	}

	TEST_CASE("A normal texture bends the way a surface faces, relative to how the texture lies on it")
	{
		struct FCase
		{
			const char* Layout;
			// The texture coordinates of the square's corners: bottom left, bottom right, top right, top left.
			FVector2f Corners[4];
			// Where "right in the picture" and "up in the picture" point on the square, as seen from the front.
			FVector PictureRight;
			FVector PictureUp;
		};

		for (const FCase& Case : {
				 FCase{"like a picture on a wall",
					 {FVector2f(0.0f, 1.0f), FVector2f(1.0f, 1.0f), FVector2f(1.0f, 0.0f), FVector2f(0.0f, 0.0f)},
					 FVector(1.0, 0.0, 0.0),
					 FVector(0.0, 1.0, 0.0)},
				 FCase{"turned a quarter turn",
					 {FVector2f(0.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(1.0f, 1.0f), FVector2f(1.0f, 0.0f)},
					 FVector(0.0, 1.0, 0.0),
					 FVector(-1.0, 0.0, 0.0)},
				 FCase{"mirrored left to right",
					 {FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f), FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f)},
					 FVector(-1.0, 0.0, 0.0),
					 FVector(0.0, 1.0, 0.0)},
				 FCase{"mirrored top to bottom and repeated three times",
					 {FVector2f(0.0f, 0.0f), FVector2f(3.0f, 0.0f), FVector2f(3.0f, 3.0f), FVector2f(0.0f, 3.0f)},
					 FVector(1.0, 0.0, 0.0),
					 FVector(0.0, -1.0, 0.0)},
			 })
		{
			for (const FColor NormalPixel : {LeanRight, LeanUp})
			{
				FSceneRenderTest Test(ESceneViewMode::Lit);
				FReferenceSurface Surface;
				Surface.BaseColor = FReferenceColor(0.8);
				WStaticMeshComponent* const Component = AddSurface(*Test.Scene.Get(), Surface);
				SetTextureCoordinates(*Component, Case.Corners[0], Case.Corners[1], Case.Corners[2], Case.Corners[3]);
				Component->Material->NormalTexture = MakeTexture(FImage(8, 8, NormalPixel), ETextureColorSpace::Linear);
				// The light comes from the upper right, so that leaning either way changes the brightness.
				const FQuat Rotation = Turn(30.0f, AxisY) * Turn(-30.0f, AxisX);
				AddSun(*Test.Scene.Get(), Rotation);

				const FImage Image = RenderPicture(Test);

				// The texture's red is along "right in the picture", its green along "up in the picture" and
				// its blue along the way the surface faces.
				const FVector Leaning = DecodeNormal(NormalPixel);
				Surface.Normal = glm::normalize(
					Case.PictureRight * Leaning.x + Case.PictureUp * Leaning.y + FVector(0.0, 0.0, Leaning.z));
				const FVector ToLight = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
				CAPTURE(Case.Layout);
				CAPTURE(static_cast<int32>(NormalPixel.R));
				CheckPixel(Image,
					Middle,
					Middle,
					ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
				CHECK(CountLoggedErrors(Test.Capture) == 0);
			}
		}
	}

	TEST_CASE("The normal scale says how strongly a normal texture bends the surface")
	{
		for (const float Scale : {0.0f, 0.5f, 1.0f, 2.0f})
		{
			FSceneRenderTest Test(ESceneViewMode::Lit);
			FReferenceSurface Surface;
			WStaticMeshComponent* const Component = AddSurface(*Test.Scene.Get(), Surface);
			Component->Material->NormalTexture = MakeTexture(FImage(8, 8, LeanRight), ETextureColorSpace::Linear);
			Component->Material->NormalScale = Scale;
			const FQuat Rotation = Turn(40.0f, AxisY);
			AddSun(*Test.Scene.Get(), Rotation);

			const FImage Image = RenderPicture(Test);

			Surface.Normal = DecodeNormal(LeanRight, static_cast<double>(Scale));
			const FVector ToLight = -FVector(Rotation * FVector3f(0.0f, 0.0f, -1.0f));
			CAPTURE(Scale);
			CheckPixel(Image,
				Middle,
				Middle,
				ReferencePixel(ReferenceReflect(Surface, ToLight, ReferenceWhite * std::numbers::pi)));
		}
	}

	TEST_CASE("A normal texture follows its surface when the surface is turned")
	{
		FSceneRenderTest Test(ESceneViewMode::Lit);
		FReferenceSurface Surface;
		// Turned about the vertical axis and tilted back, the square, its texture and the lean all turn together.
		const FQuat SquareRotation = Turn(35.0f, AxisY) * Turn(-20.0f, AxisX);
		WStaticMeshComponent* const Component =
			AddSurface(*Test.Scene.Get(), Surface, MakeTransform(SquareLocation, SquareRotation, FVector3f(1.5f)));
		Component->Material->NormalTexture = MakeTexture(FImage(8, 8, LeanUp), ETextureColorSpace::Linear);
		AddSun(*Test.Scene.Get());

		const FImage Image = RenderPicture(Test);

		const FVector3f Leaning = FVector3f(DecodeNormal(LeanUp));
		Surface.Normal = FVector(glm::normalize(SquareRotation * Leaning));
		CheckPixel(Image,
			Middle,
			Middle,
			ReferencePixel(ReferenceReflect(Surface, FVector(0.0, 0.0, 1.0), ReferenceWhite * std::numbers::pi)));
	}

	TEST_CASE("In the unlit view mode only the base colour texture shows")
	{
		FSceneRenderTest Test;
		const FImage Picture = MakeQuarters(16, Red, Green, Blue, White);
		WStaticMeshComponent* const Component = AddTexturedSquare(*Test.Scene.Get(), MakeTexture(Picture));
		Component->Material->NormalTexture = MakeTexture(FImage(8, 8, LeanRight), ETextureColorSpace::Linear);
		Component->Material->MetallicRoughnessTexture = MakeTexture(FImage(8, 8, Black), ETextureColorSpace::Linear);
		Component->Material->EmissiveTexture = MakeTexture(FImage(8, 8, White));
		Component->Material->EmissiveColor = FLinearColor(5.0f, 5.0f, 5.0f);

		const FImage Image = Test.Render(MakeWideView());

		CHECK(CutOutSquare(Image) == Picture);
		CHECK(Test.Renderer->GetStatistics().CachedTextureCount == 4);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}
}
