#include "Scene/Scene.h"

#include "Object/GarbageCollector.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** Returns the names of the given entities, which is what most tests compare. */
	std::vector<std::string> GetNames(const std::vector<WEntity*>& Entities)
	{
		std::vector<std::string> Names;
		for (const WEntity* const Entity : Entities)
		{
			Names.push_back(Entity->GetName());
		}
		return Names;
	}
}

TEST_SUITE("WScene")
{
	TEST_CASE("A new scene is empty and can be named")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		CHECK(Scene->GetName().empty());
		CHECK(Scene->GetRootEntities().empty());
		CHECK(Scene->GetAllEntities().empty());

		Scene->SetName("Level 1");
		CHECK(Scene->GetName() == "Level 1");
	}

	TEST_CASE("CreateEntity adds a named entity at the root of the scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		WEntity* const Entity = Scene->CreateEntity("Player");

		REQUIRE(Entity != nullptr);
		CHECK(Entity->GetName() == "Player");
		CHECK(Entity->GetScene() == Scene.Get());
		CHECK(Entity->GetParent() == nullptr);
		REQUIRE(Scene->GetRootEntities().size() == 1);
		CHECK(Scene->GetRootEntities()[0].Get() == Entity);
	}

	TEST_CASE("CreateEntity with a parent adds the entity as that parent's last child")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");

		WEntity* const First = Scene->CreateEntity("First", Parent);
		WEntity* const Second = Scene->CreateEntity("Second", Parent);

		REQUIRE(Parent->GetChildren().size() == 2);
		CHECK(Parent->GetChildren()[0].Get() == First);
		CHECK(Parent->GetChildren()[1].Get() == Second);
		CHECK(First->GetParent() == Parent);
		CHECK(Scene->GetRootEntities().size() == 1);
	}

	TEST_CASE("CreateEntity refuses a parent from another scene or a destroyed parent")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		const FScopedTestScene OtherScene;
		WEntity* const Foreign = OtherScene->CreateEntity("Foreign");
		WEntity* const Destroyed = Scene->CreateEntity("Destroyed");
		Destroyed->Destroy();

		CHECK(Scene->CreateEntity("Child", Foreign) == nullptr);
		CHECK(Scene->CreateEntity("Child", Destroyed) == nullptr);

		CHECK(Scene->GetAllEntities().empty());
		CHECK(Foreign->GetChildren().empty());
		CHECK(Capture.GetMessages().size() == 2);
	}

	TEST_CASE("A destroyed scene creates no entities")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		WScene* const Scene = NewObject<WScene>();
		Scene->Destroy();

		CHECK(Scene->CreateEntity("Late") == nullptr);
	}

	TEST_CASE("Every entity and component gets its own identifier, which finds it again")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const First = Scene->CreateEntity("First");
		WEntity* const Second = Scene->CreateEntity("Second", First);
		WTestComponent* const Component = Second->AddComponent<WTestComponent>();

		CHECK(First->GetId() > 0);
		CHECK(Second->GetId() > 0);
		CHECK(Component->GetId() > 0);
		CHECK(First->GetId() != Second->GetId());
		CHECK(Second->GetId() != Component->GetId());

		CHECK(Scene->FindEntityById(First->GetId()) == First);
		CHECK(Scene->FindEntityById(Second->GetId()) == Second);
		CHECK(Scene->FindComponentById(Component->GetId()) == Component);
	}

	TEST_CASE("An identifier finds only an object of the kind asked for")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestComponent* const Component = Entity->AddComponent<WTestComponent>();

		CHECK(Scene->FindComponentById(Entity->GetId()) == nullptr);
		CHECK(Scene->FindEntityById(Component->GetId()) == nullptr);
		CHECK(Scene->FindEntityById(0) == nullptr);
		CHECK(Scene->FindEntityById(-1) == nullptr);
		CHECK(Scene->FindEntityById(123456) == nullptr);
	}

	TEST_CASE("The identifier of a destroyed object finds nothing and is not given out again")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const First = Scene->CreateEntity("First");
		const int64 FirstId = First->GetId();

		First->Destroy();
		WEntity* const Second = Scene->CreateEntity("Second");

		CHECK(Scene->FindEntityById(FirstId) == nullptr);
		CHECK(Second->GetId() != FirstId);
	}

	TEST_CASE("Entities in two scenes are told apart by their scene, not by their identifier")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		const FScopedTestScene OtherScene;
		WEntity* const Entity = Scene->CreateEntity("Here");
		WEntity* const OtherEntity = OtherScene->CreateEntity("There");

		CHECK(Entity->GetId() == OtherEntity->GetId());
		CHECK(Scene->FindEntityById(Entity->GetId()) == Entity);
		CHECK(OtherScene->FindEntityById(Entity->GetId()) == OtherEntity);
	}

	TEST_CASE("GetAllEntities lists every entity, parents before children and siblings in order")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const A = Scene->CreateEntity("A");
		WEntity* const B = Scene->CreateEntity("B");
		WEntity* const A1 = Scene->CreateEntity("A1", A);
		Scene->CreateEntity("A2", A);
		Scene->CreateEntity("A1a", A1);
		Scene->CreateEntity("B1", B);

		CHECK(GetNames(Scene->GetAllEntities()) == std::vector<std::string>{"A", "A1", "A1a", "A2", "B", "B1"});
	}

	TEST_CASE("FindEntityByName returns the first entity with the name, or null")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const FirstTwin = Scene->CreateEntity("Twin", Parent);
		Scene->CreateEntity("Twin");

		CHECK(Scene->FindEntityByName("Parent") == Parent);
		CHECK(Scene->FindEntityByName("Twin") == FirstTwin);
		CHECK(Scene->FindEntityByName("twin") == nullptr);
		CHECK(Scene->FindEntityByName("") == nullptr);
	}

	TEST_CASE("GetComponents finds every component of a type, including derived types, across the scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const A = Scene->CreateEntity("A");
		WEntity* const B = Scene->CreateEntity("B", A);
		WTestComponent* const First = A->AddComponent<WTestComponent>();
		A->AddComponent<WTestOtherComponent>();
		WTestDerivedComponent* const Second = B->AddComponent<WTestDerivedComponent>();

		CHECK(Scene->GetComponents<WTestComponent>() == std::vector<WTestComponent*>{First, Second});
		CHECK(Scene->GetComponents<WTestDerivedComponent>() == std::vector<WTestDerivedComponent*>{Second});
		CHECK(Scene->GetComponents<WComponent>().size() == 3);
		CHECK(Scene->GetComponents<WTestAbstractComponent>().empty());
	}

	TEST_CASE("A scene keeps its entities and their components alive")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		const TWeakObjectPtr<WTestComponent> Component = Child->AddComponent<WTestComponent>();
		const TWeakObjectPtr<WEntity> WeakChild = Child;

		CollectGarbage();

		CHECK(WeakChild.IsValid());
		CHECK(Component.IsValid());
	}

	TEST_CASE("A scene that nothing refers to is collected with everything in it")
	{
		const FScopedSceneTest SceneTest;
		WScene* const Scene = NewObject<WScene>();
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		Child->AddComponent<WTestComponent>();
		const TWeakObjectPtr<WScene> WeakScene = Scene;
		const TWeakObjectPtr<WEntity> WeakChild = Child;
		const int32 ObjectCountBefore = FGarbageCollector::GetObjectCount();

		// The entities and components point back at each other and at the scene; none of that keeps them alive.
		CollectGarbage();

		CHECK_FALSE(WeakScene.IsValid());
		CHECK_FALSE(WeakChild.IsValid());
		CHECK(FGarbageCollector::GetObjectCount() == ObjectCountBefore - 4);
	}

	TEST_CASE("DestroyAllEntities empties the scene and leaves it usable")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		WEntity* const Other = Scene->CreateEntity("Other");
		const int64 OtherId = Other->GetId();

		Scene->DestroyAllEntities();

		CHECK_FALSE(IsValid(Parent));
		CHECK_FALSE(IsValid(Child));
		CHECK_FALSE(IsValid(Other));
		CHECK(Scene->GetRootEntities().empty());
		CHECK(Scene->FindEntityById(OtherId) == nullptr);

		WEntity* const Fresh = Scene->CreateEntity("Fresh");
		REQUIRE(Fresh != nullptr);
		CHECK(Fresh->GetId() > OtherId);
		CHECK(Scene->GetRootEntities().size() == 1);
	}

	TEST_CASE("Destroying a scene destroys everything in it")
	{
		const FScopedSceneTest SceneTest;
		WScene* const Scene = NewObject<WScene>();
		Scene->AddToRoot();
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestComponent* const Component = Entity->AddComponent<WTestComponent>();

		Scene->Destroy();

		CHECK_FALSE(IsValid(Scene));
		CHECK_FALSE(IsValid(Entity));
		CHECK_FALSE(IsValid(Component));
		CHECK_FALSE(Scene->IsRooted());
	}
}
