#include "Rendering/StaticMeshComponent.h"

#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMesh.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"

#include <doctest/doctest.h>

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
}
