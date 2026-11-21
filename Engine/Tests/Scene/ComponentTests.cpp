#include "Scene/Component.h"

#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("WComponent")
{
	TEST_CASE("AddComponent creates a component that belongs to the entity")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");

		WTestComponent* const Component = Entity->AddComponent<WTestComponent>();

		REQUIRE(Component != nullptr);
		CHECK(Component->GetOwner() == Entity);
		CHECK(Component->GetScene() == Scene.Get());
		CHECK(Component->GetClass() == WTestComponent::StaticClass());
		REQUIRE(Entity->GetComponents().size() == 1);
		CHECK(Entity->GetComponents()[0].Get() == Component);
	}

	TEST_CASE("AddComponent by class creates a component of that class")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");

		WComponent* const Component = Entity->AddComponent(WClass::FindByName("WTestDerivedComponent"));

		REQUIRE(Component != nullptr);
		CHECK(Component->IsA<WTestDerivedComponent>());
	}

	TEST_CASE("An entity can have several components, of the same class too, in the order they were added")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");

		WComponent* const First = Entity->AddComponent<WTestComponent>();
		WComponent* const Second = Entity->AddComponent<WTestOtherComponent>();
		WComponent* const Third = Entity->AddComponent<WTestComponent>();

		REQUIRE(Entity->GetComponents().size() == 3);
		CHECK(Entity->GetComponents()[0].Get() == First);
		CHECK(Entity->GetComponents()[1].Get() == Second);
		CHECK(Entity->GetComponents()[2].Get() == Third);
	}

	TEST_CASE("AddComponent refuses a class that is null, abstract or not a component")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");

		CHECK(Entity->AddComponent(nullptr) == nullptr);
		CHECK(Entity->AddComponent(WTestAbstractComponent::StaticClass()) == nullptr);
		CHECK(Entity->AddComponent(WTestObject::StaticClass()) == nullptr);
		CHECK(Entity->AddComponent(WEntity::StaticClass()) == nullptr);

		CHECK(Entity->GetComponents().empty());
		CHECK(Capture.GetMessages().size() == 4);
	}

	TEST_CASE("A destroyed entity accepts no components")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		Entity->Destroy();

		CHECK(Entity->AddComponent<WTestComponent>() == nullptr);
	}

	TEST_CASE("FindComponent returns the first component of a class or of a class derived from it")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestOtherComponent* const Other = Entity->AddComponent<WTestOtherComponent>();
		WTestDerivedComponent* const Derived = Entity->AddComponent<WTestDerivedComponent>();
		Entity->AddComponent<WTestComponent>();

		CHECK(Entity->FindComponent<WTestOtherComponent>() == Other);
		CHECK(Entity->FindComponent<WTestComponent>() == Derived);
		CHECK(Entity->FindComponent<WTestDerivedComponent>() == Derived);
		CHECK(Entity->FindComponent<WComponent>() == Other);
		CHECK(Entity->FindComponent<WTestAbstractComponent>() == nullptr);
		CHECK(Entity->FindComponent(nullptr) == nullptr);
	}

	TEST_CASE("OnAttached is called when a component is added")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");

		Entity->AddComponent<WTestComponent>();

		CHECK(FTestComponentStats::Get().Events == std::vector<std::string>{"Attached Unnamed"});
	}

	TEST_CASE("RemoveComponent detaches and destroys the component")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestComponent* const Removed = Entity->AddComponent<WTestComponent>();
		Removed->Label = "Removed";
		WTestComponent* const Kept = Entity->AddComponent<WTestComponent>();
		const int64 RemovedId = Removed->GetId();
		FTestComponentStats::Get().Events.clear();

		CHECK(Entity->RemoveComponent(Removed));

		CHECK_FALSE(IsValid(Removed));
		CHECK(Removed->GetOwner() == nullptr);
		REQUIRE(Entity->GetComponents().size() == 1);
		CHECK(Entity->GetComponents()[0].Get() == Kept);
		CHECK(Scene->FindComponentById(RemovedId) == nullptr);
		CHECK(FTestComponentStats::Get().Events == std::vector<std::string>{"Detached Removed"});
		CHECK(FTestComponentStats::Get().DetachedWithOwnerCount == 1);
	}

	TEST_CASE("Destroying a component directly removes it from its entity")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestComponent* const Component = Entity->AddComponent<WTestComponent>();

		Component->Destroy();

		CHECK(Entity->GetComponents().empty());
		CHECK(Component->GetOwner() == nullptr);
		CHECK(FTestComponentStats::Get().DetachedWithOwnerCount == 1);
	}

	TEST_CASE("RemoveComponent refuses null and a component of another entity")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WEntity* const Other = Scene->CreateEntity("Other");
		WTestComponent* const Foreign = Other->AddComponent<WTestComponent>();

		CHECK_FALSE(Entity->RemoveComponent(nullptr));
		CHECK_FALSE(Entity->RemoveComponent(Foreign));

		CHECK(IsValid(Foreign));
		CHECK(Other->GetComponents().size() == 1);
	}

	TEST_CASE("Removing a component twice succeeds only the first time")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestComponent* const Component = Entity->AddComponent<WTestComponent>();

		CHECK(Entity->RemoveComponent(Component));
		CHECK_FALSE(Entity->RemoveComponent(Component));

		CHECK(FTestComponentStats::Get().DetachedWithOwnerCount == 1);
	}

	TEST_CASE("Destroying an entity detaches and destroys its components, which still see their owner")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestComponent* const First = Entity->AddComponent<WTestComponent>();
		First->Label = "First";
		WTestComponent* const Second = Entity->AddComponent<WTestComponent>();
		Second->Label = "Second";
		FTestComponentStats::Get().Events.clear();

		Entity->Destroy();

		CHECK_FALSE(IsValid(First));
		CHECK_FALSE(IsValid(Second));
		CHECK(Entity->GetComponents().empty());
		CHECK(FTestComponentStats::Get().Events == std::vector<std::string>{"Detached First", "Detached Second"});
		CHECK(FTestComponentStats::Get().DetachedWithOwnerCount == 2);
	}

	TEST_CASE("Destroying an entity detaches the components of its descendants too")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WEntity* const Child = Scene->CreateEntity("Child", Entity);
		WTestComponent* const Component = Child->AddComponent<WTestComponent>();
		const int64 ComponentId = Component->GetId();

		Entity->Destroy();

		CHECK_FALSE(IsValid(Component));
		CHECK(Scene->FindComponentById(ComponentId) == nullptr);
		CHECK(FTestComponentStats::Get().DetachedWithOwnerCount == 1);
	}

	TEST_CASE("The properties of a component class include those of its base classes")
	{
		const WClass* const Class = WTestDerivedComponent::StaticClass();

		CHECK(Class->FindProperty("Range") != nullptr);
		CHECK(Class->FindProperty("Health") != nullptr);
		CHECK(Class->FindProperty("Owner") != nullptr);
		CHECK(Class->IsChildOf(WComponent::StaticClass()));
	}
}
