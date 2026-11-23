#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneLighting.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <limits>
#include <ostream>

// These tests draw a line around squares and look at the pixels beside them. With the wide test camera, a square
// of one metre two metres ahead covers the pixels 24 to 39 of 64 each way: the line is in the pixels just outside.

namespace
{
	/** Where the square of these tests is. */
	const FVector SquarePlace(0.0, 0.0, -2.0);

	/**
	 * Adds a square that is outlined, and returns its component. The line is blue: a colour that no square and no
	 * background of these tests has, and one that encodes to exact byte values.
	 */
	WStaticMeshComponent* AddOutlinedSquare(
		FSceneRenderTest& Test, const FTransform& Transform, const FLinearColor& Color = LinearRed)
	{
		WStaticMeshComponent* const Square = AddSquare(*Test.Scene.Get(), Transform, Color);
		Test.Renderer->OutlinedEntities.push_back(Square->GetOwner()->GetId());
		Test.Renderer->OutlineColor = LinearBlue;
		return Square;
	}
}

TEST_SUITE("FSceneRenderer outlines")
{
	TEST_CASE("Without outlined entities a picture has no lines, and an identifier that no entity has changes nothing")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(SquarePlace), LinearRed);

		const FImage Plain = Test.Render(MakeWideView());
		CHECK(Plain.GetPixel(23, 32) == Magenta);
		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 0);

		Test.Renderer->OutlinedEntities = {123456789};
		CHECK(Test.Render(MakeWideView()) == Plain);
		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("An outlined entity has a line around what it shows: outside it, and as wide as asked")
	{
		FSceneRenderTest Test;
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));
		Test.Renderer->OutlineWidth = 2.0f;

		const FImage Image = Test.Render(MakeWideView());

		// The square is as it was, out to its edges.
		for (const int32 Inside : {24, 32, 39})
		{
			CHECK(Image.GetPixel(Inside, 32) == Red);
			CHECK(Image.GetPixel(32, Inside) == Red);
		}
		// Beside each edge are two pixels of line.
		for (const int32 OnLine : {22, 23, 40, 41})
		{
			CHECK(Image.GetPixel(OnLine, 32) == Blue);
			CHECK(Image.GetPixel(32, OnLine) == Blue);
			// Along the edge too, up to its ends.
			CHECK(Image.GetPixel(OnLine, 24) == Blue);
			CHECK(Image.GetPixel(39, OnLine) == Blue);
		}
		// And then the picture is as it was.
		for (const int32 Beyond : {20, 21, 42, 43})
		{
			CHECK(Image.GetPixel(Beyond, 32) == Magenta);
			CHECK(Image.GetPixel(32, Beyond) == Magenta);
		}
		// The line goes around a corner in a curve: whole beside the corner, and gone where a square corner would
		// still have it.
		CHECK(Image.GetPixel(23, 23) == Blue);
		CHECK(Image.GetPixel(40, 40) == Blue);
		CHECK(Image.GetPixel(21, 21) == Magenta);
		CHECK(Image.GetPixel(42, 21) == Magenta);

		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 1);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The width of the line is in pixels, and its last pixel is soft where the width is not a whole number")
	{
		FSceneRenderTest Test;
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));

		Test.Renderer->OutlineWidth = 1.0f;
		const FImage Thin = Test.Render(MakeWideView());
		CHECK(Thin.GetPixel(23, 32) == Blue);
		CHECK(Thin.GetPixel(22, 32) == Magenta);

		Test.Renderer->OutlineWidth = 3.0f;
		const FImage Thick = Test.Render(MakeWideView());
		CHECK(Thick.GetPixel(21, 32) == Blue);
		CHECK(Thick.GetPixel(20, 32) == Magenta);

		// A pixel and a half: the first pixel is line, the second half line and half what was there.
		Test.Renderer->OutlineWidth = 1.5f;
		const FImage Between = Test.Render(MakeWideView());
		CHECK(Between.GetPixel(23, 32) == Blue);
		const FColor Soft = Between.GetPixel(22, 32);
		CHECK(Soft.B == 255);
		CHECK(Soft.G == 0);
		CHECK(Soft.R > 150);
		CHECK(Soft.R < 220);
		CHECK(Between.GetPixel(21, 32) == Magenta);
	}

	TEST_CASE("A line is no wider than the widest there is, and a width of nothing draws none")
	{
		FSceneRenderTest Test;
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));
		Test.Renderer->OutlinedEntities.clear();
		const FImage Plain = Test.Render(MakeWideView());
		Test.Renderer->OutlinedEntities = {Test.Scene->GetAllEntities().front()->GetId()};

		// Four pixels are the widest.
		Test.Renderer->OutlineWidth = 100.0f;
		const FImage Widest = Test.Render(MakeWideView());
		CHECK(Widest.GetPixel(20, 32) == Blue);
		CHECK(Widest.GetPixel(19, 32) == Magenta);
		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 1);

		for (const float NoWidth : {0.0f, -2.0f, std::numeric_limits<float>::quiet_NaN()})
		{
			Test.Renderer->OutlineWidth = NoWidth;
			CHECK(Test.Render(MakeWideView()) == Plain);
			CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 0);
		}
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("What is below an outlined entity is outlined with it, and what is beside it is not")
	{
		FSceneRenderTest Test;
		WEntity* const Cart = Test.Scene->CreateEntity("Cart");
		REQUIRE(Cart != nullptr);
		WEntity* const Axle = Test.Scene->CreateEntity("Axle", Cart);
		REQUIRE(Axle != nullptr);
		// A wheel two entities below the cart, and a stone that has nothing to do with it, to the right: a metre
		// and a quarter from the middle, where a square covers the pixels 44 to 59.
		AddSquare(*Test.Scene.Get(), MakeTransform(SquarePlace), LinearRed, Axle);
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(1.25, 0.0, -2.0)), LinearGreen);
		Test.Renderer->OutlinedEntities = {Cart->GetId()};
		Test.Renderer->OutlineColor = LinearBlue;

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(23, 32) == Blue);
		CHECK(Image.GetPixel(40, 32) == Blue);
		CHECK(Image.GetPixel(41, 32) == Blue);
		CHECK(Image.GetPixel(42, 32) == Magenta);
		CHECK(Image.GetPixel(43, 32) == Magenta);
		CHECK(Image.GetPixel(44, 32) == Green);
		CHECK(Image.GetPixel(60, 32) == Magenta);
		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 1);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 2);
	}

	TEST_CASE("The line goes around what is hidden too, over what hides it")
	{
		FSceneRenderTest Test;
		// Four metres ahead, a square of a metre covers the pixels 28 to 35; the green one in front hides it.
		AddOutlinedSquare(Test, MakeTransform(FVector(0.0, 0.0, -4.0)));
		AddSquare(*Test.Scene.Get(), MakeTransform(SquarePlace), LinearGreen);

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Green);
		CHECK(Image.GetPixel(28, 32) == Green);
		CHECK(Image.GetPixel(27, 32) == Blue);
		CHECK(Image.GetPixel(26, 32) == Blue);
		CHECK(Image.GetPixel(25, 32) == Green);
		CHECK(Image.GetPixel(32, 36) == Blue);
		// The square in front is not outlined.
		CHECK(Image.GetPixel(23, 32) == Magenta);
	}

	TEST_CASE("Outlined meshes that overlap have one line around them all")
	{
		FSceneRenderTest Test;
		// The second square is half a metre to the right and a little behind: it covers the pixels 32 to 47.
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));
		AddOutlinedSquare(Test, MakeTransform(FVector(0.5, 0.0, -2.02)), LinearGreen);

		const FImage Image = Test.Render(MakeWideView());

		// Where one of them alone would have its line, the other is.
		CHECK(Image.GetPixel(31, 32) == Red);
		CHECK(Image.GetPixel(40, 32) == Green);
		CHECK(Image.GetPixel(41, 32) == Green);
		// The line is around the two together.
		CHECK(Image.GetPixel(23, 32) == Blue);
		CHECK(Image.GetPixel(48, 32) == Blue);
		CHECK(Image.GetPixel(49, 32) == Blue);
		CHECK(Image.GetPixel(50, 32) == Magenta);
		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 2);
	}

	TEST_CASE("The line has its own colour in every view mode, whatever the light and the exposure")
	{
		ESceneViewMode ViewMode = ESceneViewMode::Lit;
		SUBCASE("Lit")
		{
			ViewMode = ESceneViewMode::Lit;
		}
		SUBCASE("Unlit")
		{
			ViewMode = ESceneViewMode::Unlit;
		}
		SUBCASE("Ambient occlusion")
		{
			ViewMode = ESceneViewMode::AmbientOcclusion;
		}

		FSceneRenderTest Test(ViewMode);
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));
		AddSun(*Test.Scene.Get());
		FCameraView View = MakeWideView();
		View.Exposure = 0.25f;

		const FImage Image = Test.Render(View);

		CHECK(Image.GetPixel(23, 32) == Blue);
		CHECK(Image.GetPixel(32, 41) == Blue);
		CHECK_FALSE(Image.GetPixel(21, 32) == Blue);
		CHECK_FALSE(Image.GetPixel(32, 32) == Blue);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The alpha of the line's colour says how much of the picture the line covers")
	{
		FSceneRenderTest Test;
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));
		Test.Renderer->OutlineColor = FLinearColor(0.0f, 0.0f, 1.0f, 0.5f);

		const FImage Image = Test.Render(MakeWideView());

		// Half blue and half the magenta of the background, mixed as light: half the red, which encodes to 188.
		const FColor Mixed = Image.GetPixel(23, 32);
		CHECK(Mixed.R >= 187);
		CHECK(Mixed.R <= 189);
		CHECK(Mixed.G == 0);
		CHECK(Mixed.B == 255);
		CHECK(Mixed.A == 255);
		CHECK(Image.GetPixel(32, 32) == Red);
	}

	TEST_CASE("A mesh that is not drawn from where the camera is has a line where it is all the same")
	{
		FSceneRenderTest Test;
		// Turned half around, the square shows the camera its back, which is not drawn.
		AddOutlinedSquare(Test, MakeTransform(SquarePlace, Turn(180.0f, AxisY)));

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Magenta);
		CHECK(Image.GetPixel(24, 32) == Magenta);
		CHECK(Image.GetPixel(23, 32) == Blue);
		CHECK(Image.GetPixel(40, 32) == Blue);
		CHECK(Image.GetPixel(21, 32) == Magenta);
	}

	TEST_CASE("A mirrored mesh has its line like any other")
	{
		FSceneRenderTest Test;
		AddOutlinedSquare(
			Test, MakeTransform(SquarePlace, FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(-1.0f, 1.0f, 1.0f)));

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Red);
		CHECK(Image.GetPixel(23, 32) == Blue);
		CHECK(Image.GetPixel(41, 32) == Blue);
		CHECK(Image.GetPixel(42, 32) == Magenta);
	}

	TEST_CASE("An outlined entity that shows nothing has no line")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Square = AddOutlinedSquare(Test, MakeTransform(SquarePlace));
		Square->bVisible = false;
		WEntity* const Empty = Test.Scene->CreateEntity("Empty");
		REQUIRE(Empty != nullptr);
		Test.Renderer->OutlinedEntities.push_back(Empty->GetId());

		CHECK(Test.Render(MakeWideView()) == FImage(64, 64, Magenta));
		CHECK(Test.Renderer->GetStatistics().OutlinedMeshCount == 0);
	}

	TEST_CASE("A line stops at the edge of the picture, and a mesh that fills the picture has none")
	{
		FSceneRenderTest Test;
		// Two metres to the left, the square is half outside: it covers the pixels 0 to 7.
		WStaticMeshComponent* const Square = AddOutlinedSquare(Test, MakeTransform(FVector(-2.0, 0.0, -2.0)));

		const FImage AtEdge = Test.Render(MakeWideView());
		CHECK(AtEdge.GetPixel(0, 32) == Red);
		CHECK(AtEdge.GetPixel(7, 32) == Red);
		CHECK(AtEdge.GetPixel(8, 32) == Blue);
		CHECK(AtEdge.GetPixel(9, 32) == Blue);
		CHECK(AtEdge.GetPixel(10, 32) == Magenta);
		CHECK(AtEdge.GetPixel(0, 23) == Blue);
		CHECK(AtEdge.GetPixel(0, 21) == Magenta);

		// Ten times as large and in the middle, it leaves no room for a line.
		Square->GetOwner()->SetRelativeTransform(
			MakeTransform(SquarePlace, FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(10.0f)));
		CHECK(Test.Render(MakeWideView()) == FImage(64, 64, Red));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("One renderer draws lines onto targets of different sizes, each in its own pixels")
	{
		FSceneRenderTest Test;
		AddOutlinedSquare(Test, MakeTransform(SquarePlace));

		// Twice as large a picture: the square covers the pixels 48 to 79, and the line is two pixels all the same.
		const FImage Large = Test.Render(MakeWideView(), 128, 128);
		CHECK(Large.GetPixel(48, 64) == Red);
		CHECK(Large.GetPixel(47, 64) == Blue);
		CHECK(Large.GetPixel(46, 64) == Blue);
		CHECK(Large.GetPixel(45, 64) == Magenta);

		const FImage Small = Test.Render(MakeWideView());
		CHECK(Small.GetPixel(23, 32) == Blue);
		CHECK(Small.GetPixel(22, 32) == Blue);
		CHECK(Small.GetPixel(21, 32) == Magenta);

		// Twice as wide as high, the picture shows more to the sides: the square is in the pixels 56 to 71 across.
		const FImage Wide = Test.Render(MakeWideView(), 128, 64);
		CHECK(Wide.GetPixel(56, 32) == Red);
		CHECK(Wide.GetPixel(55, 32) == Blue);
		CHECK(Wide.GetPixel(53, 32) == Magenta);
		CHECK(Wide.GetPixel(64, 22) == Blue);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}
}
