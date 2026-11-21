#pragma once

#include "CoreMinimal.h"

#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/CameraView.h"
#include "Rendering/Image.h"
#include "Rendering/Material.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/StaticMeshVertex.h"
#include "Rendering/TestRendering.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <memory>
#include <string>
#include <vector>

// Helpers for tests that draw scenes.

inline constexpr FLinearColor LinearRed(1.0f, 0.0f, 0.0f);
inline constexpr FLinearColor LinearGreen(0.0f, 1.0f, 0.0f);
inline constexpr FLinearColor LinearBlue(0.0f, 0.0f, 1.0f);
inline constexpr FLinearColor LinearBlack(0.0f, 0.0f, 0.0f);
/** The background of unlit test pictures: a colour no square has, and one that encodes to exact byte values. */
inline constexpr FLinearColor LinearMagenta(1.0f, 0.0f, 1.0f);

inline constexpr FColor Red(255, 0, 0);
inline constexpr FColor Green(0, 255, 0);
inline constexpr FColor Blue(0, 0, 255);
inline constexpr FColor White(255, 255, 255);
inline constexpr FColor Black(0, 0, 0);
inline constexpr FColor Magenta(255, 0, 255);

inline const FVector3f AxisX(1.0f, 0.0f, 0.0f);
inline const FVector3f AxisY(0.0f, 1.0f, 0.0f);
inline const FVector3f AxisZ(0.0f, 0.0f, 1.0f);

/**
 * A square of one metre in the XY plane, centred on its origin, whose front faces +Z. A texture lies on it the way
 * a picture hangs on a wall: its top left corner at the square's top left corner as seen from the front.
 */
inline WStaticMesh* MakeSquareMesh()
{
	const std::vector<FStaticMeshVertex> Vertices{
		{FVector3f(-0.5f, -0.5f, 0.0f), AxisZ, FVector2f(0.0f, 1.0f)},
		{FVector3f(0.5f, -0.5f, 0.0f), AxisZ, FVector2f(1.0f, 1.0f)},
		{FVector3f(0.5f, 0.5f, 0.0f), AxisZ, FVector2f(1.0f, 0.0f)},
		{FVector3f(-0.5f, 0.5f, 0.0f), AxisZ, FVector2f(0.0f, 0.0f)},
	};

	WStaticMesh* const Mesh = NewObject<WStaticMesh>();
	std::string Error;
	REQUIRE(Mesh->SetGeometry(Vertices, {0, 1, 2, 0, 2, 3}, Error));
	return Mesh;
}

inline WMaterial* MakeMaterial(const FLinearColor& Color)
{
	WMaterial* const Material = NewObject<WMaterial>();
	Material->BaseColor = Color;
	return Material;
}

inline FTransform MakeTransform(const FVector& Location,
	const FQuat& Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f),
	const FVector3f& Scale = FVector3f(1.0f))
{
	FTransform Transform;
	Transform.Location = Location;
	Transform.Rotation = Rotation;
	Transform.Scale = Scale;
	return Transform;
}

/** A rotation of Degrees about Axis; counter-clockwise when the axis points at the viewer. */
inline FQuat Turn(float Degrees, const FVector3f& Axis)
{
	return glm::angleAxis(glm::radians(Degrees), Axis);
}

/** Adds an entity that shows a square of the given colour, and returns its component. */
inline WStaticMeshComponent* AddSquare(
	WScene& Scene, const FTransform& Transform, const FLinearColor& Color, WEntity* Parent = nullptr)
{
	WEntity* const Entity = Scene.CreateEntity("Square", Parent);
	REQUIRE(Entity != nullptr);
	Entity->SetRelativeTransform(Transform);

	WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
	REQUIRE(Component != nullptr);
	Component->Mesh = MakeSquareMesh();
	Component->Material = MakeMaterial(Color);
	return Component;
}

/**
 * A camera with a field of view of 90 degrees. With it, a square of one metre two metres ahead covers the middle
 * quarter of the picture in each direction: pixels 24 to 39 of 64.
 */
inline FCameraView MakeWideView(const FVector& Location = FVector(0.0))
{
	FCameraView View;
	View.Location = Location;
	View.VerticalFieldOfView = 90.0f;
	return View;
}

/** The statistics of a picture in which meshes were drawn and nothing else happened. */
inline FSceneRenderStatistics MakeMeshStatistics(int32 DrawnMeshCount, int32 UploadedMeshCount, int32 CachedMeshCount)
{
	FSceneRenderStatistics Statistics;
	Statistics.DrawnMeshCount = DrawnMeshCount;
	Statistics.UploadedMeshCount = UploadedMeshCount;
	Statistics.CachedMeshCount = CachedMeshCount;
	return Statistics;
}

/** A render device, a scene renderer and an empty scene, torn down in a safe order. */
struct FSceneRenderTest
{
	/**
	 * @param ViewMode How the renderer shades. Tests of where things are drawn use the unlit mode, in which a
	 * pixel has exactly its material's colour; those get a magenta background, and lit ones a black one.
	 */
	explicit FSceneRenderTest(ESceneViewMode ViewMode = ESceneViewMode::Unlit)
	{
		std::string Error;
		Renderer = FSceneRenderer::Create(*Device, Error);
		INFO("Error: ", Error);
		REQUIRE(Renderer != nullptr);
		Renderer->ViewMode = ViewMode;
		Renderer->BackgroundColor = ViewMode == ESceneViewMode::Unlit ? LinearMagenta : LinearBlack;
	}

	/** Draws the scene to a new render target of the given size and returns the picture. */
	FImage Render(const FCameraView& View, int32 Width = 64, int32 Height = 64)
	{
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, Width, Height);
		std::string Error;
		const bool bRendered = Renderer->Render(*Scene.Get(), View, *Target, Error);
		INFO("Error: ", Error);
		REQUIRE(bRendered);
		return ReadTestTarget(*Target);
	}

	const FScopedLogCapture Capture;
	const FScopedSceneTest SceneTest;
	std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
	std::unique_ptr<FSceneRenderer> Renderer;
	const FScopedTestScene Scene;
};
