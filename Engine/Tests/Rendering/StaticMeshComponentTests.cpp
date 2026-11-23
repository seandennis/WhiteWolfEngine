#include "Rendering/StaticMeshComponent.h"

#include "Math/Box.h"
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
}
