#include "Rendering/SceneRenderer.h"

#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/CameraView.h"
#include "Rendering/Image.h"
#include "Rendering/Material.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestRendering.h"
#include "Rendering/TestSceneRendering.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("FSceneRenderer")
{
	TEST_CASE("An empty scene is drawn as the background colour")
	{
		FSceneRenderTest Test;

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image == FImage(64, 64, Magenta));
		CHECK(Test.Renderer->GetStatistics() == FSceneRenderStatistics());
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A mesh in front of the camera is drawn in its material's colour, exactly where it is")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);

		const FImage Image = Test.Render(MakeWideView());

		// The square covers pixels 24 to 39 in both directions and nothing else.
		CHECK(Image.GetPixel(32, 32) == Red);
		CHECK(Image.GetPixel(24, 24) == Red);
		CHECK(Image.GetPixel(39, 39) == Red);
		CHECK(Image.GetPixel(24, 39) == Red);
		CHECK(Image.GetPixel(39, 24) == Red);
		CHECK(Image.GetPixel(23, 32) == Magenta);
		CHECK(Image.GetPixel(40, 32) == Magenta);
		CHECK(Image.GetPixel(32, 23) == Magenta);
		CHECK(Image.GetPixel(32, 40) == Magenta);
		CHECK(Image.GetPixel(0, 0) == Magenta);
		CHECK(Image.GetPixel(63, 63) == Magenta);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Up in the world is up in the picture, and right is right")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 1.0, -2.0)), LinearRed);
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(1.0, 0.0, -2.0)), LinearGreen);

		const FImage Image = Test.Render(MakeWideView());

		// Row 0 is the top of the picture and column 0 its left edge.
		CHECK(Image.GetPixel(32, 16) == Red);
		CHECK(Image.GetPixel(32, 48) == Magenta);
		CHECK(Image.GetPixel(48, 32) == Green);
		CHECK(Image.GetPixel(16, 32) == Magenta);
	}

	TEST_CASE("The camera's rotation and location decide what is in the picture")
	{
		FSceneRenderTest Test;
		// One square to the camera's left, facing the origin, and one behind the camera.
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(-2.0, 0.0, 0.0), Turn(90.0f, AxisY)), LinearRed);
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, 12.0), Turn(180.0f, AxisY)), LinearGreen);

		// Looking ahead, neither is in view.
		CHECK(Test.Render(MakeWideView()) == FImage(64, 64, Magenta));

		// Turned a quarter turn to the left, the camera faces the red square.
		FCameraView LookingLeft = MakeWideView();
		LookingLeft.Rotation = Turn(90.0f, AxisY);
		CHECK(Test.Render(LookingLeft).GetPixel(32, 32) == Red);

		// Turned around and moved to two metres from the green square, the camera sees that one.
		FCameraView LookingBack = MakeWideView(FVector(0.0, 0.0, 10.0));
		LookingBack.Rotation = Turn(180.0f, AxisY);
		const FImage Behind = Test.Render(LookingBack);
		CHECK(Behind.GetPixel(32, 32) == Green);
		CHECK(Behind.GetPixel(24, 24) == Green);
		CHECK(Behind.GetPixel(23, 23) == Magenta);
	}

	TEST_CASE("A mesh is drawn with its entity's scale and rotation, scale first")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		WEntity* const Entity = Component->GetOwner();

		// Twice as wide: pixels 16 to 47 across, still 24 to 39 down.
		Entity->SetRelativeTransform(
			MakeTransform(FVector(0.0, 0.0, -2.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(2.0f, 1.0f, 1.0f)));
		const FImage Wide = Test.Render(MakeWideView());
		CHECK(Wide.GetPixel(17, 32) == Red);
		CHECK(Wide.GetPixel(46, 32) == Red);
		CHECK(Wide.GetPixel(32, 20) == Magenta);

		// The same, then turned a quarter turn in the picture plane: tall instead of wide.
		Entity->SetRelativeTransform(
			MakeTransform(FVector(0.0, 0.0, -2.0), Turn(90.0f, AxisZ), FVector3f(2.0f, 1.0f, 1.0f)));
		const FImage Tall = Test.Render(MakeWideView());
		CHECK(Tall.GetPixel(32, 17) == Red);
		CHECK(Tall.GetPixel(32, 46) == Red);
		CHECK(Tall.GetPixel(20, 32) == Magenta);
	}

	TEST_CASE("A child entity is drawn where its parents put it")
	{
		FSceneRenderTest Test;
		WEntity* const Parent = Test.Scene->CreateEntity("Parent");
		Parent->SetRelativeTransform(
			MakeTransform(FVector(1.0, 0.0, -2.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(2.0f)));
		// Half a metre to the parent's left, in the parent's doubled units: back in the middle, twice the size.
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(-0.5, 0.0, 0.0)), LinearGreen, Parent);

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Green);
		CHECK(Image.GetPixel(16, 16) == Green);
		CHECK(Image.GetPixel(47, 47) == Green);
		CHECK(Image.GetPixel(15, 32) == Magenta);
		CHECK(Image.GetPixel(48, 32) == Magenta);
	}

	TEST_CASE("The back of a triangle is not drawn")
	{
		FSceneRenderTest Test;
		// Turned half a turn about the vertical axis, the square faces away from the camera.
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0), Turn(180.0f, AxisY)), LinearRed);

		CHECK(Test.Render(MakeWideView()) == FImage(64, 64, Magenta));

		// Seen from the other side it is there.
		FCameraView FromBehind = MakeWideView(FVector(0.0, 0.0, -4.0));
		FromBehind.Rotation = Turn(180.0f, AxisY);
		CHECK(Test.Render(FromBehind).GetPixel(32, 32) == Red);
	}

	TEST_CASE("A nearer surface hides a farther one, whichever was added to the scene first")
	{
		for (const bool bNearFirst : {false, true})
		{
			FSceneRenderTest Test;
			const FTransform Near = MakeTransform(FVector(0.0, 0.0, -2.0));
			const FTransform Far =
				MakeTransform(FVector(0.0, 0.0, -4.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(4.0f));
			if (bNearFirst)
			{
				AddSquare(*Test.Scene.Get(), Near, LinearRed);
				AddSquare(*Test.Scene.Get(), Far, LinearGreen);
			}
			else
			{
				AddSquare(*Test.Scene.Get(), Far, LinearGreen);
				AddSquare(*Test.Scene.Get(), Near, LinearRed);
			}

			const FImage Image = Test.Render(MakeWideView());

			CAPTURE(bNearFirst);
			CHECK(Image.GetPixel(32, 32) == Red);
			CHECK(Image.GetPixel(24, 24) == Red);
			// The far square is four times the size at twice the distance, so it shows around the near one.
			CHECK(Image.GetPixel(20, 32) == Green);
			CHECK(Image.GetPixel(8, 8) == Magenta);
			CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 2);
		}
	}

	TEST_CASE("Something centimetres away and something millions of kilometres away are drawn together")
	{
		FSceneRenderTest Test;
		const FQuat NoRotation(1.0f, 0.0f, 0.0f, 0.0f);
		// A quarter of a metre from the camera, covering the middle half of the picture.
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -0.25), NoRotation, FVector3f(0.25f)), LinearRed);
		// Two million kilometres away and large enough to fill the picture. There is no far plane to cut it off.
		AddSquare(
			*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0e9), NoRotation, FVector3f(8.0e9f)), LinearGreen);
		// One part in a thousand farther still, and it must stay hidden behind the green one.
		AddSquare(
			*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.002e9), NoRotation, FVector3f(8.0e9f)), LinearBlue);

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(32, 32) == Red);
		CHECK(Image.GetPixel(16, 16) == Red);
		CHECK(Image.GetPixel(47, 47) == Red);
		CHECK(Image.GetPixel(15, 32) == Green);
		CHECK(Image.GetPixel(0, 0) == Green);
		CHECK(Image.GetPixel(63, 63) == Green);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Something closer than the near plane is not drawn")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -0.05)), LinearRed);
		FCameraView View = MakeWideView();
		View.NearPlane = 0.1f;

		CHECK(Test.Render(View) == FImage(64, 64, Magenta));

		View.NearPlane = 0.01f;
		CHECK(Test.Render(View) == FImage(64, 64, Red));
	}

	TEST_CASE("The picture is the same at the origin, a thousand and ten billion kilometres from it")
	{
		// The scene, with a hierarchy, rotations and scales, is built around a point and drawn from a camera
		// near that point. The offsets are multiples of an eighth of a metre, which double precision still
		// represents exactly ten billion kilometres out, so the pictures must be equal pixel for pixel.
		std::vector<FImage> Images;
		for (const double Distance : {0.0, 1.0e6, 1.0e13})
		{
			FSceneRenderTest Test;
			const FVector Origin(Distance, -Distance, Distance);

			WEntity* const Parent = Test.Scene->CreateEntity("Parent");
			Parent->SetRelativeTransform(
				MakeTransform(Origin + FVector(0.25, 0.0, -3.0), FQuat(1.0f, 0.0f, 0.0f, 0.0f), FVector3f(2.0f)));
			AddSquare(*Test.Scene.Get(),
				MakeTransform(FVector(0.5, 0.25, 0.125), Turn(30.0f, AxisY), FVector3f(1.5f, 0.75f, 1.0f)),
				LinearRed,
				Parent);
			AddSquare(
				*Test.Scene.Get(), MakeTransform(FVector(-0.75, -0.5, 0.25), Turn(45.0f, AxisZ)), LinearGreen, Parent);
			AddSquare(*Test.Scene.Get(),
				MakeTransform(Origin + FVector(-1.0, 0.5, -6.0), Turn(-20.0f, AxisY), FVector3f(6.0f)),
				LinearBlue);

			FCameraView View = MakeWideView(Origin + FVector(0.0, 0.25, 0.5));
			View.Rotation = Turn(10.0f, AxisY);
			View.VerticalFieldOfView = 70.0f;
			Images.push_back(Test.Render(View, 128, 96));

			CHECK(CountLoggedErrors(Test.Capture) == 0);
		}

		// The picture shows all three squares and the background; it is not equal by being empty.
		int32 RedCount = 0;
		int32 GreenCount = 0;
		int32 BlueCount = 0;
		int32 BackgroundCount = 0;
		for (const FColor& Pixel : Images[0].GetPixels())
		{
			RedCount += Pixel == Red ? 1 : 0;
			GreenCount += Pixel == Green ? 1 : 0;
			BlueCount += Pixel == Blue ? 1 : 0;
			BackgroundCount += Pixel == Magenta ? 1 : 0;
		}
		CHECK(RedCount > 100);
		CHECK(GreenCount > 100);
		CHECK(BlueCount > 100);
		CHECK(BackgroundCount > 100);
		CHECK(RedCount + GreenCount + BlueCount + BackgroundCount == 128 * 96);

		CHECK(Images[1] == Images[0]);
		CHECK(Images[2] == Images[0]);
	}

	TEST_CASE("A picture that is wider than it is high shows more to the sides, without stretching")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);

		const FImage Image = Test.Render(MakeWideView(), 128, 64);

		// The square is still 16 pixels in each direction, in the middle of the wider picture.
		CHECK(Image.GetPixel(56, 24) == Red);
		CHECK(Image.GetPixel(71, 39) == Red);
		CHECK(Image.GetPixel(55, 32) == Magenta);
		CHECK(Image.GetPixel(72, 32) == Magenta);
		CHECK(Image.GetPixel(64, 23) == Magenta);
		CHECK(Image.GetPixel(64, 40) == Magenta);
	}

	TEST_CASE("A component without a material is drawn white, and the material's alpha does not show")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);

		Component->Material = nullptr;
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == White);

		Component->Material = MakeMaterial(FLinearColor(0.0f, 0.0f, 1.0f, 0.25f));
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Blue);
	}

	TEST_CASE("A change to a material shows in the next picture")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Red);

		Component->Material->BaseColor = LinearGreen;

		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Green);
	}

	TEST_CASE("Components that are invisible, have no mesh or have a mesh without geometry are not drawn")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Invisible =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		WStaticMeshComponent* const WithoutMesh =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearGreen);
		WStaticMeshComponent* const WithoutGeometry =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearBlue);

		Invisible->bVisible = false;
		WithoutMesh->Mesh = nullptr;
		WithoutGeometry->Mesh = NewObject<WStaticMesh>();

		CHECK(Test.Render(MakeWideView()) == FImage(64, 64, Magenta));
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 0);

		Invisible->bVisible = true;
		CHECK(Test.Render(MakeWideView()).GetPixel(32, 32) == Red);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Destroyed entities and components are not drawn")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const First =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(-1.0, 0.0, -2.0)), LinearRed);
		WStaticMeshComponent* const Second =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(1.0, 0.0, -2.0)), LinearGreen);
		CHECK(Test.Render(MakeWideView()).GetPixel(16, 32) == Red);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 2);

		First->GetOwner()->Destroy();
		Second->Destroy();

		CHECK(Test.Render(MakeWideView()) == FImage(64, 64, Magenta));
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 0);
	}

	TEST_CASE("A mesh is copied to the graphics card once, however often and by however many components it is drawn")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const First =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(-1.0, 0.0, -2.0)), LinearRed);
		WStaticMeshComponent* const Second =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(1.0, 0.0, -2.0)), LinearGreen);
		Second->Mesh = First->Mesh;

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(16, 32) == Red);
		CHECK(Image.GetPixel(48, 32) == Green);
		CHECK(Test.Renderer->GetStatistics() == MakeMeshStatistics(2, 1, 1));

		Test.Render(MakeWideView());
		CHECK(Test.Renderer->GetStatistics() == MakeMeshStatistics(2, 0, 1));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("New geometry for a mesh shows in the next picture")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		CHECK(Test.Render(MakeWideView()).GetPixel(20, 32) == Magenta);

		// The same square, now two metres wide and made of more vertices and triangles than before.
		const std::vector<FStaticMeshVertex> Vertices{
			{FVector3f(-1.0f, -0.5f, 0.0f), AxisZ},
			{FVector3f(0.0f, -0.5f, 0.0f), AxisZ},
			{FVector3f(1.0f, -0.5f, 0.0f), AxisZ},
			{FVector3f(1.0f, 0.5f, 0.0f), AxisZ},
			{FVector3f(0.0f, 0.5f, 0.0f), AxisZ},
			{FVector3f(-1.0f, 0.5f, 0.0f), AxisZ},
		};
		std::string Error;
		REQUIRE(Component->Mesh->SetGeometry(Vertices, {0, 1, 4, 0, 4, 5, 1, 2, 3, 1, 3, 4}, Error));

		const FImage Image = Test.Render(MakeWideView());

		CHECK(Image.GetPixel(17, 32) == Red);
		CHECK(Image.GetPixel(46, 32) == Red);
		CHECK(Image.GetPixel(15, 32) == Magenta);
		CHECK(Test.Renderer->GetStatistics() == MakeMeshStatistics(1, 1, 1));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The copy of a mesh on the graphics card is released when the mesh is collected")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		Test.Render(MakeWideView());
		REQUIRE(Test.Renderer->GetStatistics().CachedMeshCount == 1);

		// A hidden mesh keeps its copy: it may be shown again.
		Component->bVisible = false;
		Test.Render(MakeWideView());
		CHECK(Test.Renderer->GetStatistics().CachedMeshCount == 1);

		Component->GetOwner()->Destroy();
		CollectGarbage();
		Test.Render(MakeWideView());

		CHECK(Test.Renderer->GetStatistics() == FSceneRenderStatistics());
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("One renderer draws different scenes to different targets")
	{
		FSceneRenderTest Test;
		const FScopedTestScene Other;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		AddSquare(*Other.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearBlue);
		const std::unique_ptr<FRenderTarget> Small = CreateTestTarget(*Test.Device, 32, 32);
		const std::unique_ptr<FRenderTarget> Large = CreateTestTarget(*Test.Device, 96, 48);
		std::string Error;

		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), MakeWideView(), *Small, Error));
		REQUIRE(Test.Renderer->Render(*Other.Get(), MakeWideView(), *Large, Error));

		CHECK(ReadTestTarget(*Small).GetPixel(16, 16) == Red);
		CHECK(ReadTestTarget(*Large).GetPixel(48, 24) == Blue);
		CHECK(ReadTestTarget(*Large).GetPixel(2, 2) == Magenta);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Drawing replaces what the target showed, and the target can be drawn to again and again")
	{
		FSceneRenderTest Test;
		WStaticMeshComponent* const Component =
			AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, 64, 64);
		std::string Error;
		Target->Clear(LinearBlue);

		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), MakeWideView(), *Target, Error));
		CHECK(ReadTestTarget(*Target).GetPixel(32, 32) == Red);
		CHECK(ReadTestTarget(*Target).GetPixel(2, 2) == Magenta);

		// Moved away, the square leaves nothing behind in the colour or in the depth of the next picture.
		Component->GetOwner()->SetRelativeTransform(MakeTransform(FVector(0.0, 0.0, -8.0)));
		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), MakeWideView(), *Target, Error));
		const FImage Image = ReadTestTarget(*Target);
		CHECK(Image.GetPixel(32, 32) == Red);
		CHECK(Image.GetPixel(24, 24) == Magenta);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("An invalid camera view is refused and the target is left as it was")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, 16, 16);
		Target->Clear(LinearBlue);
		FCameraView View = MakeWideView();
		View.NearPlane = 0.0f;
		std::string Error;

		CHECK_FALSE(Test.Renderer->Render(*Test.Scene.Get(), View, *Target, Error));

		CHECK(Error.starts_with("the camera view is not valid"));
		CHECK(ReadTestTarget(*Target) == FImage(16, 16, Blue));
	}

	TEST_CASE("A destroyed scene is refused")
	{
		FSceneRenderTest Test;
		WScene* const Gone = NewObject<WScene>();
		Gone->Destroy();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, 16, 16);
		std::string Error;

		CHECK_FALSE(Test.Renderer->Render(*Gone, MakeWideView(), *Target, Error));

		CHECK(Error == "the scene has been destroyed");
	}

	TEST_CASE("A scene renderer keeps the graphics card alive after its device object is gone")
	{
		FSceneRenderTest Test;
		AddSquare(*Test.Scene.Get(), MakeTransform(FVector(0.0, 0.0, -2.0)), LinearRed);
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Test.Device, 64, 64);
		std::string Error;

		Test.Device.reset();

		REQUIRE(Test.Renderer->Render(*Test.Scene.Get(), MakeWideView(), *Target, Error));
		CHECK(ReadTestTarget(*Target).GetPixel(32, 32) == Red);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}
}
