#pragma once

#include "CoreMinimal.h"

#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/CameraView.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/Image.h"
#include "Rendering/Material.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestSceneRendering.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <doctest/doctest.h>

#include <string>

// The renderer's test scene: a small set that uses everything the renderer can do, drawn in tests and compared
// with a reference image. A floor with a checkerboard texture carries three spheres (red plastic, gold, rough
// blue), a box with a coloured texture, a box with a grooved normal texture and a small glowing sphere with a
// lamp above it; a sun lights the whole.
//
// Everything is placed relative to an origin, at offsets that are multiples of an eighth of a metre. Double
// precision represents those exactly even ten billion kilometres from the world origin, so the scene is the same
// scene wherever it is built.

/** The size of the pictures taken of the test scene. */
inline constexpr int32 RendererTestSceneWidth = 320;
inline constexpr int32 RendererTestSceneHeight = 180;

/** The colour behind the test scene. */
inline constexpr FLinearColor RendererTestSceneBackground(0.02f, 0.03f, 0.05f);

/** A texture of the given picture, which a test expects to be valid. */
inline WTexture* MakeTestSceneTexture(const FImage& Image, ETextureColorSpace ColorSpace)
{
	WTexture* const Texture = NewObject<WTexture>();
	std::string Error;
	REQUIRE(Texture->SetImage(Image, ColorSpace, Error));
	return Texture;
}

/** A checkerboard of light and dark grey squares, eight squares a side. */
inline FImage MakeTestSceneCheckerboard()
{
	constexpr int32 Size = 64;
	constexpr int32 SquareSize = 8;
	FImage Image(Size, Size);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const bool bIsLight = (X / SquareSize + Y / SquareSize) % 2 == 0;
			Image.SetPixel(X, Y, bIsLight ? FColor(200, 200, 200) : FColor(90, 90, 90));
		}
	}
	return Image;
}

/** A picture whose four quarters are red, green, blue and white. */
inline FImage MakeTestSceneQuarters()
{
	constexpr int32 Size = 32;
	FImage Image(Size, Size);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const bool bIsLeft = X < Size / 2;
			Image.SetPixel(X, Y, Y < Size / 2 ? (bIsLeft ? Red : Green) : (bIsLeft ? Blue : White));
		}
	}
	return Image;
}

/** A normal texture of vertical grooves: stripes that lean left and right in turn. */
inline FImage MakeTestSceneGrooves()
{
	constexpr int32 Size = 32;
	constexpr int32 StripeWidth = 4;
	FImage Image(Size, Size);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const bool bLeansRight = (X / StripeWidth) % 2 == 0;
			Image.SetPixel(X, Y, bLeansRight ? FColor(204, 128, 230) : FColor(51, 128, 230));
		}
	}
	return Image;
}

/** Adds an entity that shows a mesh with a new material, and returns the material for the caller to fill in. */
inline WMaterial* AddTestSceneObject(
	WScene& Scene, const std::string& Name, const FTransform& Transform, WStaticMesh* Mesh, WEntity* Parent = nullptr)
{
	REQUIRE(Mesh != nullptr);
	WEntity* const Entity = Scene.CreateEntity(Name, Parent);
	REQUIRE(Entity != nullptr);
	Entity->SetRelativeTransform(Transform);

	WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
	REQUIRE(Component != nullptr);
	Component->Mesh = Mesh;
	Component->Material = NewObject<WMaterial>();
	return Component->Material.Get();
}

/**
 * Builds the renderer's test scene.
 *
 * @param Origin Where in the world the middle of the scene's floor is.
 */
inline void BuildRendererTestScene(WScene& Scene, const FVector& Origin)
{
	const FQuat NoRotation(1.0f, 0.0f, 0.0f, 0.0f);

	WMaterial* const Floor =
		AddTestSceneObject(Scene, "Floor", MakeTransform(Origin), FMeshShapes::CreatePlane(8.0f, 8.0f));
	Floor->BaseColorTexture = MakeTestSceneTexture(MakeTestSceneCheckerboard(), ETextureColorSpace::Srgb);
	Floor->Roughness = 0.9f;

	// The spheres share one mesh and hang below one parent, half a metre above the floor.
	WEntity* const Spheres = Scene.CreateEntity("Spheres");
	REQUIRE(Spheres != nullptr);
	Spheres->SetRelativeTransform(MakeTransform(Origin + FVector(0.0, 0.5, 0.0)));
	WStaticMesh* const Sphere = FMeshShapes::CreateSphere(0.5f, 48);

	WMaterial* const Plastic =
		AddTestSceneObject(Scene, "RedPlastic", MakeTransform(FVector(-1.5, 0.0, -1.0)), Sphere, Spheres);
	Plastic->BaseColor = FLinearColor(0.8f, 0.05f, 0.05f);
	Plastic->Roughness = 0.35f;

	WMaterial* const Gold = AddTestSceneObject(Scene, "Gold", MakeTransform(FVector(0.0, 0.0, -1.5)), Sphere, Spheres);
	Gold->BaseColor = FLinearColor(1.0f, 0.77f, 0.34f);
	Gold->Metallic = 1.0f;
	Gold->Roughness = 0.3f;

	WMaterial* const Chalk =
		AddTestSceneObject(Scene, "BlueChalk", MakeTransform(FVector(1.5, 0.0, -1.0)), Sphere, Spheres);
	Chalk->BaseColor = FLinearColor(0.1f, 0.2f, 0.8f);
	Chalk->Roughness = 0.9f;

	WMaterial* const Painted = AddTestSceneObject(Scene,
		"PaintedBox",
		MakeTransform(Origin + FVector(-1.0, 0.375, 0.5), Turn(30.0f, AxisY)),
		FMeshShapes::CreateBox(FVector3f(0.75f)));
	Painted->BaseColorTexture = MakeTestSceneTexture(MakeTestSceneQuarters(), ETextureColorSpace::Srgb);
	Painted->Roughness = 0.6f;

	WMaterial* const Grooved = AddTestSceneObject(Scene,
		"GroovedBox",
		MakeTransform(Origin + FVector(2.0, 0.5, 0.75), Turn(-25.0f, AxisY), FVector3f(1.0f, 1.0f, 0.5f)),
		FMeshShapes::CreateBox(FVector3f(1.0f)));
	Grooved->BaseColor = FLinearColor(0.6f, 0.6f, 0.55f);
	Grooved->NormalTexture = MakeTestSceneTexture(MakeTestSceneGrooves(), ETextureColorSpace::Linear);
	Grooved->Roughness = 0.5f;

	WMaterial* const Glowing = AddTestSceneObject(
		Scene, "Bulb", MakeTransform(Origin + FVector(0.75, 0.125, 0.75), NoRotation, FVector3f(0.25f)), Sphere);
	Glowing->BaseColor = FLinearColor(0.0f, 0.0f, 0.0f);
	Glowing->EmissiveColor = FLinearColor(4.0f, 2.0f, 0.5f);

	WEntity* const Lamp = Scene.CreateEntity("Lamp");
	REQUIRE(Lamp != nullptr);
	Lamp->SetRelativeTransform(MakeTransform(Origin + FVector(0.75, 0.75, 0.75)));
	WPointLightComponent* const LampLight = Lamp->AddComponent<WPointLightComponent>();
	REQUIRE(LampLight != nullptr);
	LampLight->Color = FLinearColor(1.0f, 0.6f, 0.3f);
	LampLight->Intensity = 1.5f;
	LampLight->Range = 6.0f;

	WEntity* const Sun = Scene.CreateEntity("Sun");
	REQUIRE(Sun != nullptr);
	Sun->SetRelativeTransform(MakeTransform(Origin, Turn(35.0f, AxisY) * Turn(-50.0f, AxisX)));
	WDirectionalLightComponent* const SunLight = Sun->AddComponent<WDirectionalLightComponent>();
	REQUIRE(SunLight != nullptr);
	SunLight->Color = FLinearColor(1.0f, 0.95f, 0.9f);
	SunLight->Intensity = 3.0f;
}

/** Returns the view the pictures of the test scene are taken from: in front of it, looking slightly down. */
inline FCameraView MakeRendererTestSceneView(const FVector& Origin)
{
	FCameraView View;
	View.Location = Origin + FVector(0.0, 1.75, 4.5);
	View.Rotation = Turn(-18.0f, AxisX);
	View.VerticalFieldOfView = 50.0f;
	return View;
}
