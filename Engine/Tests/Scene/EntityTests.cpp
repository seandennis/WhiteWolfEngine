#include "Scene/Entity.h"

#include "Object/Class.h"
#include "Object/GarbageCollector.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Reflection/Function.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** A rotation of a quarter turn about the Z axis. */
	FQuat QuarterTurnAboutZ()
	{
		const float HalfAngle = glm::radians(45.0f);
		return FQuat(std::cos(HalfAngle), 0.0f, 0.0f, std::sin(HalfAngle));
	}

	/** Rotations are single precision, so a position that went through one is exact to about seven digits. */
	bool IsNear(const FVector& Actual, const FVector& Expected)
	{
		return glm::length(Actual - Expected) < 1.0e-4;
	}
}

TEST_SUITE("WEntity")
{
	TEST_CASE("An entity can be renamed")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Old");

		Entity->SetName("New");

		CHECK(Entity->GetName() == "New");
	}

	TEST_CASE("A new entity has an identity transform")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		const WEntity* const Entity = Scene->CreateEntity("Entity");

		CHECK(Entity->GetRelativeTransform() == FTransform());
		CHECK(Entity->GetWorldTransform() == FTransform());
		CHECK(Entity->GetWorldLocation() == FVector(0.0));
	}

	TEST_CASE("The world transform of a root entity is its relative transform")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		FTransform Transform;
		Transform.Location = FVector(1.0, 2.0, 3.0);
		Transform.Scale = FVector3f(2.0f);

		Entity->SetRelativeTransform(Transform);

		CHECK(Entity->GetWorldTransform() == Transform);
		CHECK(Entity->GetWorldLocation() == FVector(1.0, 2.0, 3.0));
	}

	TEST_CASE("The world transform of a child combines the transforms of its ancestors")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Grandparent = Scene->CreateEntity("Grandparent");
		WEntity* const Parent = Scene->CreateEntity("Parent", Grandparent);
		WEntity* const Child = Scene->CreateEntity("Child", Parent);

		FTransform GrandparentTransform;
		GrandparentTransform.Location = FVector(100.0, 0.0, 0.0);
		Grandparent->SetRelativeTransform(GrandparentTransform);

		FTransform ParentTransform;
		ParentTransform.Rotation = QuarterTurnAboutZ();
		ParentTransform.Scale = FVector3f(2.0f);
		Parent->SetRelativeTransform(ParentTransform);

		FTransform ChildTransform;
		ChildTransform.Location = FVector(1.0, 0.0, 0.0);
		Child->SetRelativeTransform(ChildTransform);

		// The child's offset is scaled by two, turned from +X to +Y, and moved with the grandparent.
		CHECK(IsNear(Child->GetWorldLocation(), FVector(100.0, 2.0, 0.0)));
	}

	TEST_CASE("World positions keep their precision far from the origin")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);

		// Ten billion kilometres from the origin, in metres, with a child one millimetre away.
		const double FarAway = 1.0e13;
		Parent->SetWorldLocation(FVector(FarAway, 0.0, 0.0));
		FTransform ChildTransform;
		ChildTransform.Location = FVector(0.001, 0.0, 0.0);
		Child->SetRelativeTransform(ChildTransform);

		CHECK(Child->GetWorldLocation().x - FarAway == doctest::Approx(0.001).epsilon(0.01));
	}

	TEST_CASE("SetWorldTransform places a child in the world whatever its parent's transform")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		FTransform ParentTransform;
		ParentTransform.Location = FVector(10.0, 20.0, 30.0);
		ParentTransform.Rotation = QuarterTurnAboutZ();
		ParentTransform.Scale = FVector3f(4.0f);
		Parent->SetRelativeTransform(ParentTransform);

		Child->SetWorldLocation(FVector(-5.0, 6.0, 7.0));

		CHECK(IsNear(Child->GetWorldLocation(), FVector(-5.0, 6.0, 7.0)));
		CHECK_FALSE(IsNear(Child->GetRelativeTransform().Location, FVector(-5.0, 6.0, 7.0)));
	}

	TEST_CASE("Moving a parent moves its children in the world")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		Child->SetWorldLocation(FVector(1.0, 0.0, 0.0));

		Parent->SetWorldLocation(FVector(0.0, 50.0, 0.0));

		CHECK(IsNear(Child->GetWorldLocation(), FVector(1.0, 50.0, 0.0)));
	}

	TEST_CASE("SetParent moves an entity under another one, as its last child")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Existing = Scene->CreateEntity("Existing", Parent);
		WEntity* const Entity = Scene->CreateEntity("Entity");

		CHECK(Entity->SetParent(Parent));

		CHECK(Entity->GetParent() == Parent);
		REQUIRE(Parent->GetChildren().size() == 2);
		CHECK(Parent->GetChildren()[0].Get() == Existing);
		CHECK(Parent->GetChildren()[1].Get() == Entity);
		REQUIRE(Scene->GetRootEntities().size() == 1);
		CHECK(Scene->GetRootEntities()[0].Get() == Parent);
	}

	TEST_CASE("SetParent with null moves an entity back to the root of the scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Entity = Scene->CreateEntity("Entity", Parent);

		CHECK(Entity->SetParent(nullptr));

		CHECK(Entity->GetParent() == nullptr);
		CHECK(Parent->GetChildren().empty());
		REQUIRE(Scene->GetRootEntities().size() == 2);
		CHECK(Scene->GetRootEntities()[1].Get() == Entity);
	}

	TEST_CASE("SetParent to the current parent changes nothing")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const First = Scene->CreateEntity("First", Parent);
		WEntity* const Second = Scene->CreateEntity("Second", Parent);

		CHECK(First->SetParent(Parent));

		REQUIRE(Parent->GetChildren().size() == 2);
		CHECK(Parent->GetChildren()[0].Get() == First);
		CHECK(Parent->GetChildren()[1].Get() == Second);
	}

	TEST_CASE("SetParent keeps the entity's place in the world by default")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Entity = Scene->CreateEntity("Entity");
		Parent->SetWorldLocation(FVector(10.0, 0.0, 0.0));
		Entity->SetWorldLocation(FVector(3.0, 4.0, 5.0));

		CHECK(Entity->SetParent(Parent));

		CHECK(IsNear(Entity->GetWorldLocation(), FVector(3.0, 4.0, 5.0)));
		CHECK(IsNear(Entity->GetRelativeTransform().Location, FVector(-7.0, 4.0, 5.0)));
	}

	TEST_CASE("SetParent can keep the relative transform instead, which moves the entity in the world")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Entity = Scene->CreateEntity("Entity");
		Parent->SetWorldLocation(FVector(10.0, 0.0, 0.0));
		Entity->SetWorldLocation(FVector(3.0, 4.0, 5.0));

		CHECK(Entity->SetParent(Parent, ETransformRule::KeepRelative));

		CHECK(IsNear(Entity->GetRelativeTransform().Location, FVector(3.0, 4.0, 5.0)));
		CHECK(IsNear(Entity->GetWorldLocation(), FVector(13.0, 4.0, 5.0)));
	}

	TEST_CASE("SetParent refuses to make an entity its own ancestor")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		WEntity* const Grandparent = Scene->CreateEntity("Grandparent");
		WEntity* const Parent = Scene->CreateEntity("Parent", Grandparent);
		WEntity* const Child = Scene->CreateEntity("Child", Parent);

		CHECK_FALSE(Grandparent->SetParent(Grandparent));
		CHECK_FALSE(Grandparent->SetParent(Parent));
		CHECK_FALSE(Grandparent->SetParent(Child));

		CHECK(Grandparent->GetParent() == nullptr);
		CHECK(Child->GetParent() == Parent);
		CHECK(Scene->GetRootEntities().size() == 1);
		CHECK(Capture.GetMessages().size() == 3);
	}

	TEST_CASE("SetParent refuses a parent in another scene, a destroyed parent and a destroyed entity")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		const FScopedTestScene OtherScene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WEntity* const Foreign = OtherScene->CreateEntity("Foreign");
		WEntity* const Destroyed = Scene->CreateEntity("Destroyed");
		Destroyed->Destroy();

		CHECK_FALSE(Entity->SetParent(Foreign));
		CHECK_FALSE(Entity->SetParent(Destroyed));
		CHECK_FALSE(Destroyed->SetParent(Entity));

		CHECK(Entity->GetParent() == nullptr);
		CHECK(Entity->GetChildren().empty());
		CHECK(Foreign->GetChildren().empty());
	}

	TEST_CASE("IsDescendantOf is true for every ancestor and for nothing else")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Grandparent = Scene->CreateEntity("Grandparent");
		WEntity* const Parent = Scene->CreateEntity("Parent", Grandparent);
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		WEntity* const Other = Scene->CreateEntity("Other");

		CHECK(Child->IsDescendantOf(Parent));
		CHECK(Child->IsDescendantOf(Grandparent));
		CHECK_FALSE(Child->IsDescendantOf(Child));
		CHECK_FALSE(Child->IsDescendantOf(Other));
		CHECK_FALSE(Grandparent->IsDescendantOf(Child));
		CHECK_FALSE(Child->IsDescendantOf(nullptr));
	}

	TEST_CASE("Destroying an entity removes it from its parent and destroys its descendants")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Entity = Scene->CreateEntity("Entity", Parent);
		WEntity* const Child = Scene->CreateEntity("Child", Entity);
		WEntity* const Grandchild = Scene->CreateEntity("Grandchild", Child);
		WEntity* const Sibling = Scene->CreateEntity("Sibling", Parent);

		Entity->Destroy();

		CHECK_FALSE(IsValid(Entity));
		CHECK_FALSE(IsValid(Child));
		CHECK_FALSE(IsValid(Grandchild));
		CHECK(IsValid(Parent));
		CHECK(IsValid(Sibling));
		REQUIRE(Parent->GetChildren().size() == 1);
		CHECK(Parent->GetChildren()[0].Get() == Sibling);
		CHECK(Entity->GetScene() == nullptr);
		CHECK(Entity->GetParent() == nullptr);
	}

	TEST_CASE("Destroying a root entity removes it from the scene's root entities")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const First = Scene->CreateEntity("First");
		WEntity* const Second = Scene->CreateEntity("Second");

		First->Destroy();

		REQUIRE(Scene->GetRootEntities().size() == 1);
		CHECK(Scene->GetRootEntities()[0].Get() == Second);
	}

	TEST_CASE("A destroyed entity and everything under it is freed by the next collection")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WEntity* const Child = Scene->CreateEntity("Child", Entity);
		const TWeakObjectPtr<WTestComponent> Component = Child->AddComponent<WTestComponent>();
		const int32 ObjectCountBefore = FGarbageCollector::GetObjectCount();

		Entity->Destroy();
		CollectGarbage();

		CHECK(FGarbageCollector::GetObjectCount() == ObjectCountBefore - 3);
		CHECK_FALSE(Component.IsValid());
	}

	TEST_CASE("A reference to a destroyed entity becomes null when it is collected")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Target = Scene->CreateEntity("Target");
		WEntity* const Observer = Scene->CreateEntity("Observer");
		WTestComponent* const Component = Observer->AddComponent<WTestComponent>();
		Component->TargetEntity = Target;
		Component->Waypoints = {Target, Observer};

		Target->Destroy();
		CollectGarbage();

		CHECK(Component->TargetEntity.Get() == nullptr);
		REQUIRE(Component->Waypoints.size() == 2);
		CHECK(Component->Waypoints[0].Get() == nullptr);
		CHECK(Component->Waypoints[1].Get() == Observer);
	}

	TEST_CASE("The name and transform of an entity are reflected properties; its place in the hierarchy is not saved")
	{
		const WClass* const Class = WEntity::StaticClass();

		const FProperty* const NameProperty = Class->FindProperty("Name");
		const FProperty* const TransformProperty = Class->FindProperty("RelativeTransform");
		REQUIRE(NameProperty != nullptr);
		REQUIRE(TransformProperty != nullptr);
		CHECK_FALSE(NameProperty->HasAnyFlags(EPropertyFlags::Transient));
		CHECK(TransformProperty->GetTypeName() == "FTransform");

		for (const char* const StructuralName : {"Scene", "Parent", "Children", "Components"})
		{
			const FProperty* const Property = Class->FindProperty(StructuralName);
			INFO("Property: ", StructuralName);
			REQUIRE(Property != nullptr);
			CHECK(Property->HasAnyFlags(EPropertyFlags::Transient));
		}
	}

	TEST_CASE("Entity functions can be called through reflection")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		const FFunction* const SetWorldLocation = WEntity::StaticClass()->FindFunction("SetWorldLocation");
		const FFunction* const GetScene = WEntity::StaticClass()->FindFunction("GetScene");
		REQUIRE(SetWorldLocation != nullptr);
		REQUIRE(GetScene != nullptr);
		FVector NewLocation(7.0, 8.0, 9.0);
		const std::array<void*, 1> Arguments{&NewLocation};
		TObjectPtr<WScene> ReturnedScene;

		SetWorldLocation->Invoke(Entity, Arguments, nullptr);
		GetScene->Invoke(Entity, {}, &ReturnedScene);

		CHECK(Entity->GetWorldLocation() == FVector(7.0, 8.0, 9.0));
		CHECK(ReturnedScene.Get() == Scene.Get());
		CHECK(SetWorldLocation->HasAnyFlags(EFunctionFlags::BlueprintCallable));
		CHECK(SetWorldLocation->GetCategory() == "Transform");
	}
}
