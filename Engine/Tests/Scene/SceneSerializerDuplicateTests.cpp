#include "Scene/SceneSerializer.h"

#include "Json/JsonValue.h"
#include "Math/Transform.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** Copies an entity, which the test expects to work. */
	WEntity* Duplicate(const WEntity& Source)
	{
		WEntity* const Copy = FSceneSerializer::DuplicateEntity(Source);
		REQUIRE(Copy != nullptr);
		return Copy;
	}
}

TEST_SUITE("FSceneSerializer::DuplicateEntity")
{
	TEST_CASE("The copy of an entity has its name and its transform, and an identifier of its own")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Player = Scene->CreateEntity("Player");
		FTransform Transform;
		Transform.Location = FVector(1.0e12, 2.0, -3.0);
		Transform.Scale = FVector3f(2.0f, 3.0f, 4.0f);
		Player->SetRelativeTransform(Transform);

		const WEntity* const Copy = Duplicate(*Player);

		CHECK(Copy != Player);
		CHECK(Copy->GetName() == "Player");
		CHECK(Copy->GetRelativeTransform().Location == FVector(1.0e12, 2.0, -3.0));
		CHECK(Copy->GetRelativeTransform().Scale == FVector3f(2.0f, 3.0f, 4.0f));
		CHECK(Copy->GetId() != Player->GetId());
		CHECK(Copy->GetScene() == Scene.Get());
		CHECK(Scene->FindEntityById(Copy->GetId()) == Copy);
		CHECK(Scene->FindEntityById(Player->GetId()) == Player);
	}

	TEST_CASE("The copy becomes the last child of the original's parent, or the last entity at the root")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const First = Scene->CreateEntity("First", Parent);
		WEntity* const Second = Scene->CreateEntity("Second", Parent);

		WEntity* const CopyOfFirst = Duplicate(*First);

		CHECK(CopyOfFirst->GetParent() == Parent);
		REQUIRE(Parent->GetChildren().size() == 3);
		CHECK(Parent->GetChildren()[0] == First);
		CHECK(Parent->GetChildren()[1] == Second);
		CHECK(Parent->GetChildren()[2] == CopyOfFirst);

		WEntity* const CopyOfParent = Duplicate(*Parent);

		CHECK(CopyOfParent->GetParent() == nullptr);
		REQUIRE(Scene->GetRootEntities().size() == 2);
		CHECK(Scene->GetRootEntities()[1] == CopyOfParent);
	}

	TEST_CASE("Components are copied with the properties that are saved, and each gets an identifier of its own")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Player = Scene->CreateEntity("Player");
		WTestDerivedComponent* const Component = Player->AddComponent<WTestDerivedComponent>();
		Component->Health = 42;
		Component->Label = "Hero";
		Component->Target = FVector(1.0, 2.0, 3.0);
		Component->Range = 7.25;
		Component->CachedDistance = 99.0f;
		WTestOtherComponent* const Other = Player->AddComponent<WTestOtherComponent>();
		Other->bEnabled = false;

		const WEntity* const Copy = Duplicate(*Player);

		REQUIRE(Copy->GetComponents().size() == 2);
		const WTestDerivedComponent* const ComponentCopy = Cast<WTestDerivedComponent>(Copy->GetComponents()[0].Get());
		REQUIRE(ComponentCopy != nullptr);
		CHECK(ComponentCopy != Component);
		CHECK(ComponentCopy->Health == 42);
		CHECK(ComponentCopy->Label == "Hero");
		CHECK(ComponentCopy->Target == FVector(1.0, 2.0, 3.0));
		CHECK(ComponentCopy->Range == 7.25);
		// What is not saved with a scene is the component's own to work out again.
		CHECK(ComponentCopy->CachedDistance == 0.0f);
		CHECK(ComponentCopy->GetOwner() == Copy);
		CHECK(ComponentCopy->GetId() != Component->GetId());
		CHECK(Scene->FindComponentById(ComponentCopy->GetId()) == ComponentCopy);

		const WTestOtherComponent* const OtherCopy = Cast<WTestOtherComponent>(Copy->GetComponents()[1].Get());
		REQUIRE(OtherCopy != nullptr);
		CHECK_FALSE(OtherCopy->bEnabled);

		// The original is as it was.
		CHECK(Component->Health == 42);
		CHECK(Component->CachedDistance == 99.0f);
		CHECK(Player->GetComponents().size() == 2);
	}

	TEST_CASE("The descendants of an entity are copied with it, in their order")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Player = Scene->CreateEntity("Player");
		WEntity* const Arm = Scene->CreateEntity("Arm", Player);
		Scene->CreateEntity("Hand", Arm)->AddComponent<WTestComponent>()->Label = "Grip";
		Scene->CreateEntity("Sword", Player);
		const std::size_t EntityCountBefore = Scene->GetAllEntities().size();

		const WEntity* const Copy = Duplicate(*Player);

		CHECK(Scene->GetAllEntities().size() == 2 * EntityCountBefore);
		REQUIRE(Copy->GetChildren().size() == 2);
		const WEntity* const ArmCopy = Copy->GetChildren()[0];
		CHECK(ArmCopy->GetName() == "Arm");
		CHECK(ArmCopy != Arm);
		CHECK(ArmCopy->GetParent() == Copy);
		CHECK(Copy->GetChildren()[1]->GetName() == "Sword");
		REQUIRE(ArmCopy->GetChildren().size() == 1);
		const WEntity* const HandCopy = ArmCopy->GetChildren()[0];
		CHECK(HandCopy->GetName() == "Hand");
		REQUIRE(HandCopy->GetComponents().size() == 1);
		CHECK(Cast<WTestComponent>(HandCopy->GetComponents()[0].Get())->Label == "Grip");

		// The original tree has the children it had.
		CHECK(Player->GetChildren().size() == 2);
		CHECK(Arm->GetChildren().size() == 1);
	}

	TEST_CASE("A copy refers to the copies of what was copied with it, and to the same objects otherwise")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Enemy = Scene->CreateEntity("Enemy");
		WTestComponent* const EnemyComponent = Enemy->AddComponent<WTestComponent>();
		WEntity* const Player = Scene->CreateEntity("Player");
		WEntity* const Arm = Scene->CreateEntity("Arm", Player);
		WTestComponent* const ArmComponent = Arm->AddComponent<WTestComponent>();
		WTestComponent* const PlayerComponent = Player->AddComponent<WTestComponent>();
		PlayerComponent->TargetEntity = Arm;
		PlayerComponent->Partner = ArmComponent;
		PlayerComponent->Waypoints = {Enemy, Player, Arm, nullptr};
		ArmComponent->TargetEntity = Enemy;
		ArmComponent->Partner = EnemyComponent;

		const WEntity* const Copy = Duplicate(*Player);

		const WTestComponent* const PlayerCopy = Copy->FindComponent<WTestComponent>();
		REQUIRE(PlayerCopy != nullptr);
		REQUIRE(Copy->GetChildren().size() == 1);
		WEntity* const ArmCopy = Copy->GetChildren()[0];
		WTestComponent* const ArmComponentCopy = ArmCopy->FindComponent<WTestComponent>();
		REQUIRE(ArmComponentCopy != nullptr);

		// Inside the copied tree: the copies.
		CHECK(PlayerCopy->TargetEntity == ArmCopy);
		CHECK(PlayerCopy->Partner.Get() == ArmComponentCopy);
		REQUIRE(PlayerCopy->Waypoints.size() == 4);
		CHECK(PlayerCopy->Waypoints[0] == Enemy);
		CHECK(PlayerCopy->Waypoints[1] == Copy);
		CHECK(PlayerCopy->Waypoints[2] == ArmCopy);
		CHECK(PlayerCopy->Waypoints[3] == nullptr);

		// Outside it: what the original refers to.
		CHECK(ArmComponentCopy->TargetEntity == Enemy);
		CHECK(ArmComponentCopy->Partner.Get() == EnemyComponent);

		// The originals refer to what they referred to.
		CHECK(PlayerComponent->TargetEntity == Arm);
		CHECK(PlayerComponent->Partner.Get() == ArmComponent);
		CHECK(PlayerComponent->Waypoints[1] == Player);
	}

	TEST_CASE("A copy shares the assets of the original, also ones that were not loaded from a file")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		WEntity* const Crate = Scene->CreateEntity("Crate");
		WStaticMeshComponent* const Component = Crate->AddComponent<WStaticMeshComponent>();
		Component->Mesh = NewObject<WStaticMesh>();
		Component->bCastShadows = false;

		const WEntity* const Copy = Duplicate(*Crate);

		const WStaticMeshComponent* const ComponentCopy = Copy->FindComponent<WStaticMeshComponent>();
		REQUIRE(ComponentCopy != nullptr);
		CHECK(ComponentCopy->Mesh == Component->Mesh);
		CHECK(ComponentCopy->Mesh != nullptr);
		CHECK(ComponentCopy->Material == nullptr);
		CHECK_FALSE(ComponentCopy->bCastShadows);
		// Nothing had to be written as a path, so nothing was warned about.
		CHECK(Capture.GetMessages().empty());
	}

	TEST_CASE("Components are told that they are attached once every property of every copy is set")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Player = Scene->CreateEntity("Player");
		Player->AddComponent<WTestComponent>()->Label = "Body";
		Scene->CreateEntity("Arm", Player)->AddComponent<WTestComponent>()->Label = "Arm";
		FTestComponentStats::Get().Events.clear();

		Duplicate(*Player);

		// Each copy was attached once, and knew its label by then.
		const std::vector<std::string> ExpectedEvents = {"Attached Body", "Attached Arm"};
		CHECK(FTestComponentStats::Get().Events == ExpectedEvents);
	}

	TEST_CASE("A component of a class that is not available is copied as the scene file has it")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		std::string Error;
		WScene* const Scene = FSceneSerializer::LoadFromText(
			R"({
				"Format": "WhiteWolfScene",
				"Version": 1,
				"NextId": 3,
				"Entities": [
					{
						"Id": 1,
						"Properties": { "Name": "Car" },
						"Components": [ { "Id": 2, "Class": "WWheelComponent", "Properties": { "Radius": 0.35 } } ]
					}
				]
			})",
			Error);
		REQUIRE(Scene != nullptr);
		Scene->AddToRoot();
		const WEntity* const Car = Scene->FindEntityById(1);
		REQUIRE(Car != nullptr);

		const WEntity* const Copy = Duplicate(*Car);

		REQUIRE(Copy->GetComponents().size() == 1);
		const WMissingComponent* const Missing = Cast<WMissingComponent>(Copy->GetComponents()[0].Get());
		REQUIRE(Missing != nullptr);
		CHECK(Missing->GetMissingClassName() == "WWheelComponent");
		const FJsonValue* const Radius = Missing->GetSavedProperties().FindMember("Radius");
		REQUIRE(Radius != nullptr);
		CHECK(Radius->GetNumber() == 0.35);
		CHECK(Missing->GetId() != 2);

		Scene->Destroy();
	}

	TEST_CASE("A copy can be copied, and every copy is its own")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Player = Scene->CreateEntity("Player");
		Player->AddComponent<WTestComponent>()->Health = 7;

		WEntity* const First = Duplicate(*Player);
		WEntity* const Second = Duplicate(*First);
		First->FindComponent<WTestComponent>()->Health = 8;
		First->SetName("Changed");

		CHECK(Player->GetName() == "Player");
		CHECK(Second->GetName() == "Player");
		CHECK(Player->FindComponent<WTestComponent>()->Health == 7);
		CHECK(Second->FindComponent<WTestComponent>()->Health == 7);
		CHECK(First->GetId() != Second->GetId());
		CHECK(Second->GetId() != Player->GetId());

		// Destroying a copy leaves the others.
		First->Destroy();
		CollectGarbage();
		CHECK(IsValid(Player));
		CHECK(IsValid(Second));
		CHECK(Scene->GetRootEntities().size() == 2);
	}

	TEST_CASE("An entity that has been destroyed cannot be copied")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		WEntity* const Player = Scene->CreateEntity("Player");
		Player->Destroy();

		CHECK(FSceneSerializer::DuplicateEntity(*Player) == nullptr);

		CHECK(Scene->GetAllEntities().empty());
		REQUIRE(Capture.GetMessages().size() == 1);
		CHECK(Capture.GetMessages()[0].Verbosity == ELogVerbosity::Warning);
	}
}
