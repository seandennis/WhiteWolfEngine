#include "Rendering/SceneRayCast.h"

#include "Math/Ray.h"
#include "Math/Transform.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/SceneRayHit.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"

#include <doctest/doctest.h>

#include <optional>

namespace
{
	/** Puts a cube of two metres into a scene, with its middle at a place, and returns the component that shows it. */
	WStaticMeshComponent* AddCube(WScene& Scene, const char* Name, const FVector& Location, WEntity* Parent = nullptr)
	{
		WEntity* const Entity = Scene.CreateEntity(Name, Parent);
		REQUIRE(Entity != nullptr);
		Entity->SetWorldLocation(Location);
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Component != nullptr);
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));
		return Component;
	}

	/** Returns the ray that begins at a point and goes along -Z, the way that a camera looks. */
	FRay MakeRayAhead(const FVector& Origin)
	{
		FRay Ray;
		Ray.Origin = Origin;
		Ray.Direction = FVector3f(0.0f, 0.0f, -1.0f);
		return Ray;
	}
}

TEST_SUITE("FSceneRayCast")
{
	TEST_CASE("A ray through a scene with nothing in it meets nothing")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		CHECK_FALSE(FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0))).has_value());

		// Nor in a scene whose entities show nothing.
		Scene->CreateEntity("Empty");
		Scene->CreateEntity("Without A Mesh")->AddComponent<WStaticMeshComponent>();
		CHECK_FALSE(FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0))).has_value());
	}

	TEST_CASE("A ray meets the mesh that is in its way, at its distance")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		const WStaticMeshComponent* const Crate = AddCube(*Scene.Get(), "Crate", FVector(5.0, 0.0, -20.0));
		AddCube(*Scene.Get(), "Elsewhere", FVector(-30.0, 0.0, -20.0));

		const std::optional<FSceneRayHit> Hit =
			FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(5.5, 0.5, 10.0)));

		REQUIRE(Hit.has_value());
		CHECK(Hit->Component == Crate);
		// The near side of the crate is a metre before its middle.
		CHECK(Hit->Distance == doctest::Approx(29.0));

		// Between the two, the ray meets neither.
		CHECK_FALSE(FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(-10.0, 0.0, 10.0))).has_value());
	}

	TEST_CASE("Of the meshes that a ray meets, the nearest is the one, whichever was made first")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		const WStaticMeshComponent* Nearest = nullptr;

		SUBCASE("The nearest was made last")
		{
			AddCube(*Scene.Get(), "Far", FVector(0.0, 0.0, -40.0));
			AddCube(*Scene.Get(), "Middle", FVector(0.0, 0.0, -20.0));
			Nearest = AddCube(*Scene.Get(), "Near", FVector(0.0, 0.0, -5.0));
		}
		SUBCASE("The nearest was made first")
		{
			Nearest = AddCube(*Scene.Get(), "Near", FVector(0.0, 0.0, -5.0));
			AddCube(*Scene.Get(), "Far", FVector(0.0, 0.0, -40.0));
			AddCube(*Scene.Get(), "Middle", FVector(0.0, 0.0, -20.0));
		}

		const std::optional<FSceneRayHit> Hit =
			FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0)));

		REQUIRE(Hit.has_value());
		CHECK(Hit->Component == Nearest);
		CHECK(Hit->Distance == doctest::Approx(14.0));
	}

	TEST_CASE("A mesh behind where the ray begins is not in its way")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		AddCube(*Scene.Get(), "Behind", FVector(0.0, 0.0, 20.0));
		const WStaticMeshComponent* const Ahead = AddCube(*Scene.Get(), "Ahead", FVector(0.0, 0.0, -20.0));

		const std::optional<FSceneRayHit> Hit =
			FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0)));

		REQUIRE(Hit.has_value());
		CHECK(Hit->Component == Ahead);
	}

	TEST_CASE("A ray goes through what is not visible, and meets what is behind it")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WStaticMeshComponent* const Hidden = AddCube(*Scene.Get(), "Hidden", FVector(0.0, 0.0, -5.0));
		const WStaticMeshComponent* const Shown = AddCube(*Scene.Get(), "Shown", FVector(0.0, 0.0, -20.0));
		Hidden->bVisible = false;

		const std::optional<FSceneRayHit> Hit =
			FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0)));

		REQUIRE(Hit.has_value());
		CHECK(Hit->Component == Shown);
		CHECK(Hit->Distance == doctest::Approx(29.0));
	}

	TEST_CASE("The meshes of entities below others are met where their parents put them")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Cart = Scene->CreateEntity("Cart");
		REQUIRE(Cart != nullptr);
		const WStaticMeshComponent* const Load = AddCube(*Scene.Get(), "Load", FVector(0.0, 0.0, 0.0), Cart);

		// The cart goes somewhere, and takes its load with it.
		Cart->SetWorldLocation(FVector(100.0, 50.0, -20.0));

		CHECK_FALSE(FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0))).has_value());
		const std::optional<FSceneRayHit> Hit =
			FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(100.0, 50.0, 10.0)));
		REQUIRE(Hit.has_value());
		CHECK(Hit->Component == Load);
		CHECK(Hit->Distance == doctest::Approx(29.0));
	}

	TEST_CASE("An entity with several meshes is met at the nearest of them")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Tower");
		REQUIRE(Entity != nullptr);
		WStaticMeshComponent* const Small = Entity->AddComponent<WStaticMeshComponent>();
		WStaticMeshComponent* const Large = Entity->AddComponent<WStaticMeshComponent>();
		Small->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));
		Large->Mesh = FMeshShapes::CreateBox(FVector3f(6.0f));

		const std::optional<FSceneRayHit> Hit =
			FSceneRayCast::FindFirstHit(*Scene.Get(), MakeRayAhead(FVector(0.0, 0.0, 10.0)));

		// The side of the large box hides the small one inside it.
		REQUIRE(Hit.has_value());
		CHECK(Hit->Component == Large);
		CHECK(Hit->Distance == doctest::Approx(7.0));
	}
}
