#include "Rendering/UiRenderer.h"

#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Rendering/Image.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/TestRendering.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureColorSpace.h"
#include "Rendering/UiDrawCommand.h"
#include "Rendering/UiDrawList.h"
#include "Rendering/UiVertex.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

// A user interface is drawn in the pixels of a surface, so what a test draws is known pixel for pixel: a rectangle
// from (4, 4) to (12, 8) covers the pixels 4 to 11 across and 4 to 7 down, and no others.

namespace
{
	constexpr FColor Red(255, 0, 0);
	constexpr FColor Green(0, 255, 0);
	constexpr FColor Blue(0, 0, 255);
	constexpr FColor White(255, 255, 255);
	constexpr FColor Black(0, 0, 0);

	/** The size of the surface most tests draw on. */
	constexpr int32 Size = 16;

	/** A render device, a renderer of user interfaces for it, and what was logged while a test ran. */
	struct FUiRenderTest
	{
		FUiRenderTest()
			: Device(CreateValidatedDevice())
		{
			std::string Error;
			Renderer = FUiRenderer::Create(*Device, Error);
			INFO("Error: ", Error);
			REQUIRE(Renderer != nullptr);
		}

		/** Draws a list onto a new surface of one colour, which a test expects to work, and returns the picture. */
		FImage Draw(const FUiDrawList& List, const FLinearColor& Background = FLinearColor(0.0f, 0.0f, 0.0f))
		{
			const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, Size, Size);
			Target->Clear(Background);

			std::string Error;
			const bool bDrawn = Renderer->Draw(List, *Target, Error);
			INFO("Error: ", Error);
			REQUIRE(bDrawn);
			return ReadTestTarget(*Target);
		}

		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		std::unique_ptr<FRenderDevice> Device;
		std::unique_ptr<FUiRenderer> Renderer;
	};

	/**
	 * Appends a rectangle to a list as a command of its own, clipped to the whole surface. The texture, if there
	 * is one, lies on it once: its top left corner at the rectangle's top left corner.
	 */
	FUiDrawCommand& AddRectangle(FUiDrawList& List,
		float Left,
		float Top,
		float Right,
		float Bottom,
		FColor Color,
		const WTexture* Texture = nullptr)
	{
		FUiDrawCommand Command;
		Command.FirstIndex = static_cast<int32>(List.Indices.size());
		Command.IndexCount = 6;
		Command.VertexOffset = static_cast<int32>(List.Vertices.size());
		Command.ClipRight = Size;
		Command.ClipBottom = Size;
		Command.Texture = Texture;

		List.Vertices.push_back({FVector2f(Left, Top), FVector2f(0.0f, 0.0f), Color});
		List.Vertices.push_back({FVector2f(Right, Top), FVector2f(1.0f, 0.0f), Color});
		List.Vertices.push_back({FVector2f(Right, Bottom), FVector2f(1.0f, 1.0f), Color});
		List.Vertices.push_back({FVector2f(Left, Bottom), FVector2f(0.0f, 1.0f), Color});
		for (const uint32 Index : {0u, 1u, 2u, 0u, 2u, 3u})
		{
			List.Indices.push_back(Index);
		}

		List.Commands.push_back(Command);
		return List.Commands.back();
	}

	/**
	 * Returns how many pixels of a picture are not the colour they should be: Inside within the given rectangle
	 * of pixels, Outside everywhere else.
	 */
	int32 CountWrongPixels(
		const FImage& Image, int32 Left, int32 Top, int32 Right, int32 Bottom, FColor Inside, FColor Outside)
	{
		int32 WrongCount = 0;
		for (int32 Y = 0; Y < Image.GetHeight(); ++Y)
		{
			for (int32 X = 0; X < Image.GetWidth(); ++X)
			{
				const bool bIsInside = X >= Left && X < Right && Y >= Top && Y < Bottom;
				if (Image.GetPixel(X, Y) != (bIsInside ? Inside : Outside))
				{
					++WrongCount;
				}
			}
		}
		return WrongCount;
	}

	WTexture* MakeTexture(const FImage& Image, ETextureColorSpace ColorSpace = ETextureColorSpace::Srgb)
	{
		WTexture* const Texture = NewObject<WTexture>();
		std::string Error;
		REQUIRE(Texture->SetImage(Image, ColorSpace, Error));
		return Texture;
	}

	/** Returns the byte that an amount of light is stored as: the sRGB encoding. */
	int32 EncodeSrgb(double Light)
	{
		const double Encoded = Light <= 0.0031308 ? Light * 12.92 : 1.055 * std::pow(Light, 1.0 / 2.4) - 0.055;
		return static_cast<int32>(std::lround(Encoded * 255.0));
	}

	/** Returns the amount of light that a byte of a colour stands for. */
	double DecodeSrgb(int32 Byte)
	{
		const double Encoded = Byte / 255.0;
		return Encoded <= 0.04045 ? Encoded / 12.92 : std::pow((Encoded + 0.055) / 1.055, 2.4);
	}
}

TEST_SUITE("FUiRenderer")
{
	TEST_CASE("A rectangle covers exactly the pixels it lies on")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		AddRectangle(List, 4.0f, 4.0f, 12.0f, 8.0f, Red);

		const FImage Image = Test.Draw(List);

		CHECK(CountWrongPixels(Image, 4, 4, 12, 8, Red, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Positions count from the top left corner of the surface")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 1.0f, 1.0f, Red);
		AddRectangle(List, 15.0f, 0.0f, 16.0f, 1.0f, Green);
		AddRectangle(List, 0.0f, 15.0f, 1.0f, 16.0f, Blue);

		const FImage Image = Test.Draw(List);

		CHECK(Image.GetPixel(0, 0) == Red);
		CHECK(Image.GetPixel(15, 0) == Green);
		CHECK(Image.GetPixel(0, 15) == Blue);
		CHECK(Image.GetPixel(15, 15) == Black);
		CHECK(Image.GetPixel(1, 1) == Black);
	}

	TEST_CASE("The colour of a vertex is sRGB-encoded: it is the colour that is read back")
	{
		FUiRenderTest Test;
		const FColor Color(128, 64, 200);
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, Color);

		const FImage Image = Test.Draw(List);

		INFO(
			"Drawn: ", int(Image.GetPixel(8, 8).R), " ", int(Image.GetPixel(8, 8).G), " ", int(Image.GetPixel(8, 8).B));
		CHECK(IsWithinOne(Image.GetPixel(8, 8), Color));
		CHECK(IsWithinOne(Image.GetPixel(0, 15), Color));
	}

	TEST_CASE("A colour covers what lies behind it by its alpha, in linear light")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		// Black that covers 128 parts of 255, on white: what is left is 127 parts of 255 of white's light.
		AddRectangle(List, 0.0f, 0.0f, 8.0f, 16.0f, FColor(0, 0, 0, 128));
		// And a colour over another colour: each contributes its share of light.
		AddRectangle(List, 8.0f, 0.0f, 16.0f, 16.0f, FColor(255, 0, 0, 64));

		const FImage Image = Test.Draw(List, FLinearColor(1.0f, 1.0f, 1.0f));

		const int32 Grey = EncodeSrgb(127.0 / 255.0);
		const FColor Left = Image.GetPixel(4, 8);
		INFO("Left: ", int(Left.R), " expected ", Grey);
		CHECK(std::abs(Left.R - Grey) <= 1);
		CHECK(Left.R == Left.G);
		CHECK(Left.G == Left.B);

		// Red stays full; green and blue keep 191 parts of 255 of white's.
		const int32 Faded = EncodeSrgb(191.0 / 255.0);
		const FColor Right = Image.GetPixel(12, 8);
		INFO("Right: ", int(Right.R), " ", int(Right.G), " expected 255 ", Faded);
		CHECK(Right.R == 255);
		CHECK(std::abs(Right.G - Faded) <= 1);
		CHECK(std::abs(Right.B - Faded) <= 1);
	}

	TEST_CASE("A colour with no alpha draws nothing, and one with full alpha covers everything")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 8.0f, 16.0f, FColor(255, 0, 0, 0));
		AddRectangle(List, 8.0f, 0.0f, 16.0f, 16.0f, FColor(0, 255, 0, 255));

		const FImage Image = Test.Draw(List, FLinearColor(0.0f, 0.0f, 1.0f));

		CHECK(CountWrongPixels(Image, 8, 0, 16, 16, Green, Blue) == 0);
	}

	TEST_CASE("Commands are drawn in order, each over the ones before it")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 12.0f, 12.0f, Red);
		AddRectangle(List, 4.0f, 4.0f, 16.0f, 16.0f, Green);

		const FImage Image = Test.Draw(List);

		CHECK(Image.GetPixel(2, 2) == Red);
		CHECK(Image.GetPixel(8, 8) == Green);
		CHECK(Image.GetPixel(14, 14) == Green);
		CHECK(Image.GetPixel(14, 2) == Black);
	}

	TEST_CASE("A command draws nothing outside its clip rectangle")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		FUiDrawCommand& Command = AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, Red);
		Command.ClipLeft = 2;
		Command.ClipTop = 3;
		Command.ClipRight = 6;
		Command.ClipBottom = 9;

		const FImage Image = Test.Draw(List);

		CHECK(CountWrongPixels(Image, 2, 3, 6, 9, Red, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A clip rectangle that reaches beyond the surface is cut off at its edges")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		FUiDrawCommand& Command = AddRectangle(List, -20.0f, -20.0f, 40.0f, 40.0f, Red);
		Command.ClipLeft = -100;
		Command.ClipTop = 10;
		Command.ClipRight = 100;
		Command.ClipBottom = 100;

		const FImage Image = Test.Draw(List);

		CHECK(CountWrongPixels(Image, 0, 10, 16, 16, Red, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A command with an empty clip rectangle, or one that lies outside the surface, draws nothing")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		FUiDrawCommand Command = AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, Red);
		List.Commands.clear();

		SUBCASE("No width")
		{
			Command.ClipLeft = 5;
			Command.ClipRight = 5;
		}
		SUBCASE("Right of left")
		{
			Command.ClipLeft = 9;
			Command.ClipRight = 4;
		}
		SUBCASE("Beyond the right edge")
		{
			Command.ClipLeft = 16;
			Command.ClipRight = 30;
		}
		SUBCASE("Above the top edge")
		{
			Command.ClipTop = -10;
			Command.ClipBottom = 0;
		}
		List.Commands.push_back(Command);

		const FImage Image = Test.Draw(List);

		CHECK(CountWrongPixels(Image, 0, 0, 0, 0, Red, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A texture covers a rectangle pixel for pixel and multiplies its colour")
	{
		FUiRenderTest Test;
		// Sixteen by sixteen pixels in four colours, each its own in every pixel of its quarter.
		FImage Picture(Size, Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const bool bIsLeft = X < Size / 2;
				const bool bIsTop = Y < Size / 2;
				Picture.SetPixel(X, Y, bIsTop ? (bIsLeft ? Red : Green) : (bIsLeft ? Blue : White));
			}
		}
		const WTexture* const Texture = MakeTexture(Picture);
		FUiDrawList List;

		SUBCASE("With a white rectangle the picture is shown as it is")
		{
			AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, White, Texture);

			const FImage Image = Test.Draw(List);

			CHECK(Image.GetPixels() == Picture.GetPixels());
		}
		SUBCASE("A coloured rectangle keeps of each pixel what the colour has")
		{
			AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, FColor(255, 255, 0), Texture);

			const FImage Image = Test.Draw(List);

			CHECK(Image.GetPixel(2, 2) == Red);
			CHECK(Image.GetPixel(12, 2) == Green);
			CHECK(Image.GetPixel(2, 12) == Black);
			CHECK(Image.GetPixel(12, 12) == FColor(255, 255, 0));
		}
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The alpha of a texture lets what lies behind it show")
	{
		FUiRenderTest Test;
		// White everywhere, as the picture of a font is, but only its left half covers anything.
		FImage Picture(Size, Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				Picture.SetPixel(X, Y, FColor(255, 255, 255, X < Size / 2 ? 255 : 0));
			}
		}
		const WTexture* const Texture = MakeTexture(Picture, ETextureColorSpace::Linear);
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, Red, Texture);

		const FImage Image = Test.Draw(List, FLinearColor(0.0f, 0.0f, 1.0f));

		CHECK(CountWrongPixels(Image, 0, 0, 8, 16, Red, Blue) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A texture that is null or has no picture leaves the colours as they are")
	{
		FUiRenderTest Test;
		const WTexture* const Empty = NewObject<WTexture>();
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 8.0f, 16.0f, Red, nullptr);
		AddRectangle(List, 8.0f, 0.0f, 16.0f, 16.0f, Green, Empty);

		const FImage Image = Test.Draw(List);

		CHECK(CountWrongPixels(Image, 0, 0, 8, 16, Red, Green) == 0);
	}

	TEST_CASE("A texture whose picture is replaced is drawn with the new picture")
	{
		FUiRenderTest Test;
		WTexture* const Texture = MakeTexture(FImage(4, 4, Red));
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, White, Texture);
		REQUIRE(Test.Draw(List).GetPixel(8, 8) == Red);

		std::string Error;
		REQUIRE(Texture->SetImage(FImage(4, 4, Green), ETextureColorSpace::Srgb, Error));

		CHECK(CountWrongPixels(Test.Draw(List), 0, 0, 16, 16, Green, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A render target is shown as a picture, and covers what lies behind whatever its alpha")
	{
		FUiRenderTest Test;
		// A surface of eight by eight pixels, green on the left and blue on the right, with no alpha at all.
		const std::unique_ptr<FRenderTarget> Source = CreateTestTarget(*Test.Device, 8, 8);
		Source->Clear(FLinearColor(0.0f, 1.0f, 0.0f, 0.0f));
		{
			FUiDrawList Half;
			FUiDrawCommand& Command = AddRectangle(Half, 4.0f, 0.0f, 8.0f, 8.0f, Blue);
			Command.ClipRight = 8;
			Command.ClipBottom = 8;
			std::string Error;
			REQUIRE(Test.Renderer->Draw(Half, *Source, Error));
		}

		FUiDrawList List;
		FUiDrawCommand& Command = AddRectangle(List, 4.0f, 4.0f, 12.0f, 12.0f, White);
		Command.RenderTarget = Source.get();

		const FImage Image = Test.Draw(List, FLinearColor(1.0f, 0.0f, 0.0f));

		// Shown pixel for pixel at (4, 4): green from 4 to 7 across, blue from 8 to 11, and red around it.
		int32 WrongCount = 0;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const bool bIsInside = X >= 4 && X < 12 && Y >= 4 && Y < 12;
				const FColor Expected = bIsInside ? (X < 8 ? Green : Blue) : Red;
				if (Image.GetPixel(X, Y) != Expected)
				{
					++WrongCount;
				}
			}
		}
		CHECK(WrongCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A render target can be drawn to again after it has been shown")
	{
		FUiRenderTest Test;
		const std::unique_ptr<FRenderTarget> Source = CreateTestTarget(*Test.Device, Size, Size);
		Source->Clear(FLinearColor(0.0f, 1.0f, 0.0f));
		FUiDrawList List;
		FUiDrawCommand& Command = AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, White);
		Command.RenderTarget = Source.get();
		REQUIRE(Test.Draw(List).GetPixel(8, 8) == Green);

		Source->Clear(FLinearColor(0.0f, 0.0f, 1.0f));

		CHECK(Test.Draw(List).GetPixel(8, 8) == Blue);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A command draws its own part of the list: its indices, on its own vertices")
	{
		FUiRenderTest Test;
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 8.0f, 8.0f, Red);
		AddRectangle(List, 8.0f, 8.0f, 16.0f, 16.0f, Green);
		AddRectangle(List, 0.0f, 8.0f, 8.0f, 16.0f, Blue);
		// Only the second of the three is drawn: its indices are 6 to 11 and count from vertex 4.
		const FUiDrawCommand Second = List.Commands[1];
		REQUIRE(Second.FirstIndex == 6);
		REQUIRE(Second.VertexOffset == 4);
		List.Commands = {Second};

		const FImage Image = Test.Draw(List);

		CHECK(CountWrongPixels(Image, 8, 8, 16, 16, Green, Black) == 0);
	}

	TEST_CASE("A list larger than any before it is drawn whole")
	{
		FUiRenderTest Test;
		// A rectangle for every pixel of the surface, many times over: more vertices than the renderer starts
		// with room for. The last rectangle on each pixel is green.
		FUiDrawList List;
		for (int32 Layer = 0; Layer < 8; ++Layer)
		{
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					const float Left = static_cast<float>(X);
					const float Top = static_cast<float>(Y);
					AddRectangle(List, Left, Top, Left + 1.0f, Top + 1.0f, Layer == 7 ? Green : Red);
				}
			}
		}
		REQUIRE(List.Vertices.size() == 8192);

		CHECK(CountWrongPixels(Test.Draw(List), 0, 0, 16, 16, Green, Black) == 0);

		// And a small list afterwards still is.
		FUiDrawList Small;
		AddRectangle(Small, 4.0f, 4.0f, 12.0f, 8.0f, Blue);
		CHECK(CountWrongPixels(Test.Draw(Small), 4, 4, 12, 8, Blue, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A list without commands, or with a command without indices, draws nothing and succeeds")
	{
		FUiRenderTest Test;
		FUiDrawList List;

		SUBCASE("An empty list")
		{
		}
		SUBCASE("Vertices and indices that no command uses")
		{
			AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, Red);
			List.Commands.clear();
		}
		SUBCASE("A command without indices")
		{
			AddRectangle(List, 0.0f, 0.0f, 16.0f, 16.0f, Red);
			List.Commands[0].IndexCount = 0;
		}

		CHECK(CountWrongPixels(Test.Draw(List), 0, 0, 0, 0, Red, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A list that is not consistent is refused, and nothing is drawn")
	{
		FUiRenderTest Test;
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, Size, Size);
		Target->Clear(FLinearColor(0.0f, 0.0f, 0.0f));
		FUiDrawList List;
		AddRectangle(List, 0.0f, 0.0f, 8.0f, 8.0f, Red);
		AddRectangle(List, 8.0f, 8.0f, 16.0f, 16.0f, Red);
		FUiDrawCommand& Command = List.Commands[1];
		const std::unique_ptr<FRenderTarget> Other = CreateTestTarget(*Test.Device, 4, 4);
		std::string ExpectedError;

		SUBCASE("An index count that is not three for each triangle")
		{
			Command.IndexCount = 5;
			ExpectedError = "command 1 of the user interface has 5 indices, which is not three for each triangle";
		}
		SUBCASE("More indices than the list has")
		{
			Command.IndexCount = 9;
			ExpectedError = "command 1 of the user interface uses 9 indices from index 6 on, and the list has 12";
		}
		SUBCASE("A first index beyond the list")
		{
			Command.FirstIndex = 30;
			ExpectedError = "command 1 of the user interface uses 6 indices from index 30 on, and the list has 12";
		}
		SUBCASE("A vertex offset that leads beyond the vertices")
		{
			Command.VertexOffset = 5;
			ExpectedError = "command 1 of the user interface uses vertex 8, and the list has 8";
		}
		SUBCASE("An index beyond the vertices")
		{
			List.Indices[7] = 4000000000u;
			ExpectedError = "command 1 of the user interface uses vertex 4000000004, and the list has 8";
		}
		SUBCASE("A negative first index")
		{
			Command.FirstIndex = -6;
			ExpectedError = "command 1 of the user interface has a negative first index, index count or vertex offset";
		}
		SUBCASE("A negative index count")
		{
			Command.IndexCount = -3;
			ExpectedError = "command 1 of the user interface has a negative first index, index count or vertex offset";
		}
		SUBCASE("A negative vertex offset")
		{
			Command.VertexOffset = -1;
			ExpectedError = "command 1 of the user interface has a negative first index, index count or vertex offset";
		}
		SUBCASE("A texture and a render target together")
		{
			Command.Texture = MakeTexture(FImage(2, 2, Green));
			Command.RenderTarget = Other.get();
			ExpectedError = "command 1 of the user interface has both a texture and a render target";
		}
		SUBCASE("The render target it is drawn onto")
		{
			Command.RenderTarget = Target.get();
			ExpectedError = "command 1 of the user interface shows the render target that it is drawn onto";
		}

		std::string Error;
		CHECK_FALSE(Test.Renderer->Draw(List, *Target, Error));
		CHECK(Error == ExpectedError);
		// Not even the first command, which was in order, was drawn.
		CHECK(CountWrongPixels(ReadTestTarget(*Target), 0, 0, 0, 0, Red, Black) == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Drawing leaves what the surface showed outside the triangles")
	{
		FUiRenderTest Test;
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, Size, Size);
		Target->Clear(FLinearColor(0.0f, 0.0f, 1.0f));
		std::string Error;
		FUiDrawList First;
		AddRectangle(First, 0.0f, 0.0f, 8.0f, 16.0f, Red);
		REQUIRE(Test.Renderer->Draw(First, *Target, Error));

		FUiDrawList Second;
		AddRectangle(Second, 0.0f, 0.0f, 16.0f, 4.0f, Green);
		REQUIRE(Test.Renderer->Draw(Second, *Target, Error));

		const FImage Image = ReadTestTarget(*Target);
		CHECK(Image.GetPixel(2, 2) == Green);
		CHECK(Image.GetPixel(12, 2) == Green);
		CHECK(Image.GetPixel(2, 10) == Red);
		CHECK(Image.GetPixel(12, 10) == Blue);
	}

	TEST_CASE("The values that decode and encode a colour in these tests agree with each other")
	{
		// The two functions the tests above calculate with are each other's inverse for every byte.
		for (int32 Byte = 0; Byte <= 255; ++Byte)
		{
			CHECK(EncodeSrgb(DecodeSrgb(Byte)) == Byte);
		}
	}
}
