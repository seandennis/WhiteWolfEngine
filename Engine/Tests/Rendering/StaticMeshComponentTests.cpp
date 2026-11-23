#include "Rendering/StaticMeshComponent.h"

#include "Math/Box.h"
#include "Math/Ray.h"
#include "Math/Transform.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/Material.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/StaticMesh.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <optional>

namespace
{
	// The two ways that the rays of these tests go.
	const FVector3f AlongMinusZ(0.0f, 0.0f, -1.0f);
	const FVector3f AlongPlusZ(0.0f, 0.0f, 1.0f);

	FRay MakeRay(const FVector& Origin, const FVector3f& Direction)
	{
		FRay Ray;
		Ray.Origin = Origin;
		Ray.Direction = Direction;
		return Ray;
	}
}

TEST_SUITE("WStaticMeshComponent")
{
	TEST_CASE("A new static mesh component is visible and has no mesh or material")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Rock");

		const WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();

		REQUIRE(Component != nullptr);
		CHECK(Component->Mesh == nullptr);
		CHECK(Component->Material == nullptr);
		CHECK(Component->bVisible);
	}

	TEST_CASE("A component keeps its mesh and material alive for as long as it is in a scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Rock");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		Component->Mesh = NewObject<WStaticMesh>();
		Component->Material = NewObject<WMaterial>();
		const TWeakObjectPtr<WStaticMesh> Mesh(Component->Mesh.Get());
		const TWeakObjectPtr<WMaterial> Material(Component->Material.Get());

		CollectGarbage();
		CHECK(Mesh.Get() != nullptr);
		CHECK(Material.Get() != nullptr);

		Entity->Destroy();
		CollectGarbage();
		CHECK(Mesh.Get() == nullptr);
		CHECK(Material.Get() == nullptr);
	}

	TEST_CASE("The world bounds of a component are where its entity puts the box of its mesh")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Crate");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f, 4.0f, 6.0f));

		// Where the entity is, as large as the mesh.
		Entity->SetWorldLocation(FVector(100.0, 0.0, -50.0));
		FBox Bounds = Component->GetWorldBounds();
		REQUIRE_FALSE(Bounds.bIsEmpty);
		CHECK(Bounds.Min == FVector(99.0, -2.0, -53.0));
		CHECK(Bounds.Max == FVector(101.0, 2.0, -47.0));

		// Scaled with the entity.
		FTransform Transform = Entity->GetRelativeTransform();
		Transform.Scale = FVector3f(2.0f, 1.0f, 0.5f);
		Entity->SetRelativeTransform(Transform);
		Bounds = Component->GetWorldBounds();
		CHECK(Bounds.Min == FVector(98.0, -2.0, -51.5));
		CHECK(Bounds.Max == FVector(102.0, 2.0, -48.5));

		// Turned a quarter around the up axis, its width and its depth change places.
		Transform.Scale = FVector3f(1.0f);
		Transform.Rotation = glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 1.0f, 0.0f));
		Entity->SetRelativeTransform(Transform);
		Bounds = Component->GetWorldBounds();
		constexpr double Tolerance = 1.0e-5;
		CHECK(Bounds.Min.x == doctest::Approx(97.0).epsilon(Tolerance));
		CHECK(Bounds.Max.x == doctest::Approx(103.0).epsilon(Tolerance));
		CHECK(Bounds.Min.y == doctest::Approx(-2.0).epsilon(Tolerance));
		CHECK(Bounds.Max.y == doctest::Approx(2.0).epsilon(Tolerance));
		CHECK(Bounds.Min.z == doctest::Approx(-51.0).epsilon(Tolerance));
		CHECK(Bounds.Max.z == doctest::Approx(-49.0).epsilon(Tolerance));
	}

	TEST_CASE("The world bounds follow the entities above the component's own")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Cart");
		WEntity* const Child = Scene->CreateEntity("Wheel", Parent);
		WStaticMeshComponent* const Component = Child->AddComponent<WStaticMeshComponent>();
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		FTransform ChildTransform;
		ChildTransform.Location = FVector(1.0, 0.0, 0.0);
		Child->SetRelativeTransform(ChildTransform);

		Parent->SetWorldLocation(FVector(10.0, 20.0, 30.0));

		const FBox Bounds = Component->GetWorldBounds();
		CHECK(Bounds.Min == FVector(10.5, 19.5, 29.5));
		CHECK(Bounds.Max == FVector(11.5, 20.5, 30.5));
	}

	TEST_CASE("A component without a mesh, or with a mesh without geometry, has no bounds")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Rock");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();

		CHECK(Component->GetWorldBounds().bIsEmpty);

		Component->Mesh = NewObject<WStaticMesh>();
		CHECK(Component->GetWorldBounds().bIsEmpty);

		// And none once it has left its entity: there is no place for the mesh to be.
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		CHECK_FALSE(Component->GetWorldBounds().bIsEmpty);
		Entity->RemoveComponent(Component);
		CHECK(Component->GetWorldBounds().bIsEmpty);
	}

	TEST_CASE("A ray meets the mesh of a component where its entity puts it, at its distance in metres")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Crate");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));
		Entity->SetWorldLocation(FVector(100.0, 0.0, -50.0));

		// From ten metres in front of the middle of the crate, whose near side is a metre before its middle.
		const std::optional<double> Distance =
			Component->IntersectRay(MakeRay(FVector(100.0, 0.0, -40.0), AlongMinusZ));

		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(9.0));

		// Past the crate, and with the crate behind the ray.
		CHECK_FALSE(Component->IntersectRay(MakeRay(FVector(103.0, 0.0, -40.0), AlongMinusZ)).has_value());
		CHECK_FALSE(Component->IntersectRay(MakeRay(FVector(100.0, 0.0, -40.0), AlongPlusZ)).has_value());
	}

	TEST_CASE("The distance to a scaled mesh is in metres of the world, not of the mesh")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Crate");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));
		FTransform Transform;
		Transform.Scale = FVector3f(1.0f, 0.5f, 3.0f);
		Entity->SetRelativeTransform(Transform);

		// Three times as deep, the crate's near side is three metres before its middle.
		const std::optional<double> Distance = Component->IntersectRay(MakeRay(FVector(0.0, 0.0, 10.0), AlongMinusZ));
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(7.0));

		// Half as high, it is no longer where the ray passes three quarters of a metre up.
		CHECK_FALSE(Component->IntersectRay(MakeRay(FVector(0.0, 0.75, 10.0), AlongMinusZ)).has_value());
	}

	TEST_CASE("A ray meets a mesh that is turned, and one below a parent, where they are")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Cart");
		WEntity* const Child = Scene->CreateEntity("Load", Parent);
		WStaticMeshComponent* const Component = Child->AddComponent<WStaticMeshComponent>();
		// Two metres wide, four high and six deep.
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f, 4.0f, 6.0f));
		Parent->SetWorldLocation(FVector(10.0, 20.0, 30.0));

		// Turned a quarter around the up axis by its parent, its width is what faces the ray.
		FTransform ParentTransform = Parent->GetRelativeTransform();
		ParentTransform.Rotation = glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 1.0f, 0.0f));
		Parent->SetRelativeTransform(ParentTransform);

		const std::optional<double> Distance = Component->IntersectRay(MakeRay(FVector(10.0, 20.0, 40.0), AlongMinusZ));
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(9.0).epsilon(1.0e-5));

		// And it is six metres wide now: a ray two and a half metres to the side still meets it.
		CHECK(Component->IntersectRay(MakeRay(FVector(12.5, 20.0, 40.0), AlongMinusZ)).has_value());
		CHECK_FALSE(Component->IntersectRay(MakeRay(FVector(13.5, 20.0, 40.0), AlongMinusZ)).has_value());
	}

	TEST_CASE("A ray meets a mesh exactly however far both are from the origin of the world")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Probe");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		// A million kilometres out, where single precision counts in steps of some sixty metres.
		const FVector Far(1.0e9, -1.0e9, 1.0e9);
		Entity->SetWorldLocation(Far);

		const std::optional<double> Distance =
			Component->IntersectRay(MakeRay(Far + FVector(0.25, 0.25, 10.0), AlongMinusZ));
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(9.5).epsilon(1.0e-6));

		// A box of a metre is missed by a ray sixty centimetres beside its middle.
		CHECK_FALSE(Component->IntersectRay(MakeRay(Far + FVector(0.6, 0.0, 10.0), AlongMinusZ)).has_value());
	}

	TEST_CASE("A mirrored mesh is met on the side that is its front after the mirroring")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Sign");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		// A floor of a metre, which faces up.
		Component->Mesh = FMeshShapes::CreatePlane(1.0f, 1.0f);
		const FVector3f Down(0.0f, -1.0f, 0.0f);
		const FVector3f Up(0.0f, 1.0f, 0.0f);
		REQUIRE(Component->IntersectRay(MakeRay(FVector(0.0, 5.0, 0.0), Down)).has_value());
		REQUIRE_FALSE(Component->IntersectRay(MakeRay(FVector(0.0, -5.0, 0.0), Up)).has_value());

		// Mirrored from top to bottom, it faces down.
		FTransform Transform;
		Transform.Scale = FVector3f(1.0f, -1.0f, 1.0f);
		Entity->SetRelativeTransform(Transform);

		CHECK_FALSE(Component->IntersectRay(MakeRay(FVector(0.0, 5.0, 0.0), Down)).has_value());
		const std::optional<double> Distance = Component->IntersectRay(MakeRay(FVector(0.0, -5.0, 0.0), Up));
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0));
	}

	TEST_CASE("A component that shows nothing is met by nothing")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Crate");
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		const FRay Ray = MakeRay(FVector(0.0, 0.0, 10.0), AlongMinusZ);

		// Without a mesh, and with a mesh that has no geometry.
		CHECK_FALSE(Component->IntersectRay(Ray).has_value());
		Component->Mesh = NewObject<WStaticMesh>();
		CHECK_FALSE(Component->IntersectRay(Ray).has_value());

		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));
		REQUIRE(Component->IntersectRay(Ray).has_value());

		// Not visible.
		Component->bVisible = false;
		CHECK_FALSE(Component->IntersectRay(Ray).has_value());
		Component->bVisible = true;

		// With a mesh that has been destroyed, and is there until it is collected.
		WStaticMesh* const Doomed = FMeshShapes::CreateBox(FVector3f(2.0f));
		Component->Mesh = Doomed;
		REQUIRE(Component->IntersectRay(Ray).has_value());
		Doomed->Destroy();
		CHECK_FALSE(Component->IntersectRay(Ray).has_value());
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));

		// Scaled to nothing along an axis.
		FTransform Transform;
		Transform.Scale = FVector3f(1.0f, 1.0f, 0.0f);
		Entity->SetRelativeTransform(Transform);
		CHECK_FALSE(Component->IntersectRay(Ray).has_value());
		Entity->SetRelativeTransform(FTransform());
		REQUIRE(Component->IntersectRay(Ray).has_value());

		// No longer on an entity: there is no place for the mesh to be.
		Entity->RemoveComponent(Component);
		CHECK_FALSE(Component->IntersectRay(Ray).has_value());
	}
}
