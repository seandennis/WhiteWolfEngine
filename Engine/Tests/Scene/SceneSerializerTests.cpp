#include "Scene/SceneSerializer.h"

#include "Core/FileHelper.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Object/GarbageCollector.h"
#include "Object/ObjectGlobals.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Loads a scene that the test expects to be valid, and returns the warnings through OutWarnings. */
	WScene* LoadValid(std::string_view Text, std::vector<std::string>& OutWarnings)
	{
		std::string Error;
		WScene* const Scene = FSceneSerializer::LoadFromText(Text, Error, &OutWarnings);
		INFO("Error: ", Error);
		REQUIRE(Scene != nullptr);
		return Scene;
	}

	/** Loads a scene that the test expects to be valid and free of warnings. */
	WScene* LoadClean(std::string_view Text)
	{
		std::vector<std::string> Warnings;
		WScene* const Scene = LoadValid(Text, Warnings);
		for (const std::string& Warning : Warnings)
		{
			INFO("Warning: ", Warning);
			CHECK(false);
		}
		return Scene;
	}

	/** Tries to load a scene that the test expects to be refused, and returns the error. */
	std::string LoadInvalid(std::string_view Text)
	{
		const int32 ObjectCountBefore = FGarbageCollector::GetObjectCount();
		std::string Error;
		WScene* const Scene = FSceneSerializer::LoadFromText(Text, Error);
		INFO("Text: ", std::string(Text));
		REQUIRE(Scene == nullptr);
		CHECK_FALSE(Error.empty());

		// A refused scene leaves nothing behind.
		CollectGarbage();
		CHECK(FGarbageCollector::GetObjectCount() == ObjectCountBefore);
		return Error;
	}

	bool Mentions(const std::string& Message, std::string_view Text)
	{
		return Message.find(Text) != std::string::npos;
	}

	/** Wraps entity records in a valid scene document. */
	std::string SceneWithEntities(std::string_view Entities)
	{
		return R"({"Format": "WhiteWolfScene", "Version": 1, "Entities": [)" + std::string(Entities) + "]}";
	}

	/** Builds a scene with a hierarchy, transforms, and components that refer to each other. */
	void FillSampleScene(WScene& Scene)
	{
		Scene.SetName("Sample");

		WEntity* const Player = Scene.CreateEntity("Player");
		FTransform PlayerTransform;
		PlayerTransform.Location = FVector(1.0e12, 2.5, -3.0);
		PlayerTransform.Rotation = FQuat(0.5f, 0.5f, 0.5f, 0.5f);
		PlayerTransform.Scale = FVector3f(2.0f, 2.0f, 2.0f);
		Player->SetRelativeTransform(PlayerTransform);

		WEntity* const Camera = Scene.CreateEntity("Camera", Player);
		WEntity* const Enemy = Scene.CreateEntity("Enemy");

		WTestComponent* const PlayerComponent = Player->AddComponent<WTestComponent>();
		PlayerComponent->Health = 75;
		PlayerComponent->Label = "Hero";
		PlayerComponent->Target = FVector(10.0, 20.0, 30.0);
		PlayerComponent->TargetEntity = Enemy;
		PlayerComponent->Waypoints = {Camera, nullptr, Enemy};
		PlayerComponent->CachedDistance = 123.0f;

		WTestDerivedComponent* const EnemyComponent = Enemy->AddComponent<WTestDerivedComponent>();
		EnemyComponent->Label = "Villain";
		EnemyComponent->Range = 7.25;
		EnemyComponent->Partner = PlayerComponent;
		PlayerComponent->Partner = EnemyComponent;

		Enemy->AddComponent<WTestOtherComponent>()->bEnabled = false;
	}
}

TEST_SUITE("FSceneSerializer")
{
	TEST_CASE("An empty scene is saved as a document with its format, version and name")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		Scene->SetName("Empty");

		const std::string Expected = "{\n"
									 "  \"Format\": \"WhiteWolfScene\",\n"
									 "  \"Version\": 1,\n"
									 "  \"NextId\": 1,\n"
									 "  \"Properties\": {\n"
									 "    \"Name\": \"Empty\"\n"
									 "  },\n"
									 "  \"Entities\": []\n"
									 "}\n";
		CHECK(FSceneSerializer::SaveToText(*Scene.Get()) == Expected);
	}

	TEST_CASE("A scene file lists entities with their parent, properties and components")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		Scene->SetName("Level");
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		FTransform ChildTransform;
		ChildTransform.Location = FVector(1.0, 2.0, 3.0);
		Child->SetRelativeTransform(ChildTransform);
		WTestOtherComponent* const Component = Child->AddComponent<WTestOtherComponent>();
		Component->bEnabled = false;

		const std::string Expected = "{\n"
									 "  \"Format\": \"WhiteWolfScene\",\n"
									 "  \"Version\": 1,\n"
									 "  \"NextId\": 4,\n"
									 "  \"Properties\": {\n"
									 "    \"Name\": \"Level\"\n"
									 "  },\n"
									 "  \"Entities\": [\n"
									 "    {\n"
									 "      \"Id\": 1,\n"
									 "      \"Properties\": {\n"
									 "        \"Name\": \"Parent\",\n"
									 "        \"RelativeTransform\": {\n"
									 "          \"Location\": [0.0, 0.0, 0.0],\n"
									 "          \"Rotation\": [1.0, 0.0, 0.0, 0.0],\n"
									 "          \"Scale\": [1.0, 1.0, 1.0]\n"
									 "        }\n"
									 "      },\n"
									 "      \"Components\": []\n"
									 "    },\n"
									 "    {\n"
									 "      \"Id\": 2,\n"
									 "      \"Parent\": 1,\n"
									 "      \"Properties\": {\n"
									 "        \"Name\": \"Child\",\n"
									 "        \"RelativeTransform\": {\n"
									 "          \"Location\": [1.0, 2.0, 3.0],\n"
									 "          \"Rotation\": [1.0, 0.0, 0.0, 0.0],\n"
									 "          \"Scale\": [1.0, 1.0, 1.0]\n"
									 "        }\n"
									 "      },\n"
									 "      \"Components\": [\n"
									 "        {\n"
									 "          \"Id\": 3,\n"
									 "          \"Class\": \"WTestOtherComponent\",\n"
									 "          \"Properties\": {\n"
									 "            \"bEnabled\": false\n"
									 "          }\n"
									 "        }\n"
									 "      ]\n"
									 "    }\n"
									 "  ]\n"
									 "}\n";
		CHECK(FSceneSerializer::SaveToText(*Scene.Get()) == Expected);
	}

	TEST_CASE("A loaded scene has the same entities, hierarchy, transforms and component data")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		FillSampleScene(*Original.Get());

		const WScene* const Loaded = LoadClean(FSceneSerializer::SaveToText(*Original.Get()));

		CHECK(Loaded->GetName() == "Sample");
		REQUIRE(Loaded->GetRootEntities().size() == 2);
		const WEntity* const Player = Loaded->GetRootEntities()[0];
		const WEntity* const Enemy = Loaded->GetRootEntities()[1];
		CHECK(Player->GetName() == "Player");
		CHECK(Enemy->GetName() == "Enemy");
		CHECK(Player->GetScene() == Loaded);
		CHECK(Player->GetRelativeTransform().Location == FVector(1.0e12, 2.5, -3.0));
		CHECK(Player->GetRelativeTransform().Rotation == FQuat(0.5f, 0.5f, 0.5f, 0.5f));
		CHECK(Player->GetRelativeTransform().Scale == FVector3f(2.0f, 2.0f, 2.0f));

		REQUIRE(Player->GetChildren().size() == 1);
		const WEntity* const Camera = Player->GetChildren()[0];
		CHECK(Camera->GetName() == "Camera");
		CHECK(Camera->GetParent() == Player);

		const WTestComponent* const PlayerComponent = Player->FindComponent<WTestComponent>();
		REQUIRE(PlayerComponent != nullptr);
		CHECK(PlayerComponent->GetOwner() == Player);
		CHECK(PlayerComponent->Health == 75);
		CHECK(PlayerComponent->Label == "Hero");
		CHECK(PlayerComponent->Target == FVector(10.0, 20.0, 30.0));

		REQUIRE(Enemy->GetComponents().size() == 2);
		const WTestDerivedComponent* const EnemyComponent = Enemy->FindComponent<WTestDerivedComponent>();
		REQUIRE(EnemyComponent != nullptr);
		CHECK(EnemyComponent->Label == "Villain");
		CHECK(EnemyComponent->Range == 7.25);
		const WTestOtherComponent* const OtherComponent = Enemy->FindComponent<WTestOtherComponent>();
		REQUIRE(OtherComponent != nullptr);
		CHECK_FALSE(OtherComponent->bEnabled);
	}

	TEST_CASE("References are loaded as references to the objects of the new scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		FillSampleScene(*Original.Get());

		const WScene* const Loaded = LoadClean(FSceneSerializer::SaveToText(*Original.Get()));

		const WEntity* const Player = Loaded->FindEntityByName("Player");
		const WEntity* const Camera = Loaded->FindEntityByName("Camera");
		const WEntity* const Enemy = Loaded->FindEntityByName("Enemy");
		REQUIRE(Player != nullptr);
		REQUIRE(Camera != nullptr);
		REQUIRE(Enemy != nullptr);
		const WTestComponent* const PlayerComponent = Player->FindComponent<WTestComponent>();
		const WTestDerivedComponent* const EnemyComponent = Enemy->FindComponent<WTestDerivedComponent>();
		REQUIRE(PlayerComponent != nullptr);
		REQUIRE(EnemyComponent != nullptr);

		CHECK(PlayerComponent->TargetEntity.Get() == Enemy);
		REQUIRE(PlayerComponent->Waypoints.size() == 3);
		CHECK(PlayerComponent->Waypoints[0].Get() == Camera);
		CHECK(PlayerComponent->Waypoints[1].Get() == nullptr);
		CHECK(PlayerComponent->Waypoints[2].Get() == Enemy);

		// The two components refer to each other; the first of them is loaded before the second exists.
		CHECK(PlayerComponent->Partner.Get() == EnemyComponent);
		CHECK(EnemyComponent->Partner.Get() == PlayerComponent);
		CHECK(PlayerComponent->TargetEntity.Get() != Original->FindEntityByName("Enemy"));
	}

	TEST_CASE("Saving a loaded scene gives exactly the text it was loaded from")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		FillSampleScene(*Original.Get());
		const std::string FirstText = FSceneSerializer::SaveToText(*Original.Get());

		const WScene* const Loaded = LoadClean(FirstText);

		CHECK(FSceneSerializer::SaveToText(*Loaded) == FirstText);
	}

	TEST_CASE("Identifiers survive a save and load, and are still never reused")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		WEntity* const Kept = Original->CreateEntity("Kept");
		WEntity* const Removed = Original->CreateEntity("Removed");
		const int64 KeptId = Kept->GetId();
		const int64 RemovedId = Removed->GetId();
		Removed->Destroy();

		WScene* const Loaded = LoadClean(FSceneSerializer::SaveToText(*Original.Get()));

		REQUIRE(Loaded->FindEntityById(KeptId) != nullptr);
		CHECK(Loaded->FindEntityById(KeptId)->GetName() == "Kept");
		const WEntity* const Fresh = Loaded->CreateEntity("Fresh");
		CHECK(Fresh->GetId() > RemovedId);
	}

	TEST_CASE("A transient property is not saved and keeps its default when loaded")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		Original->CreateEntity("Entity")->AddComponent<WTestComponent>()->CachedDistance = 55.0f;

		const std::string Text = FSceneSerializer::SaveToText(*Original.Get());
		const WScene* const Loaded = LoadClean(Text);

		CHECK_FALSE(Mentions(Text, "CachedDistance"));
		CHECK_FALSE(Mentions(Text, "Owner"));
		const std::vector<WTestComponent*> Components = Loaded->GetComponents<WTestComponent>();
		REQUIRE(Components.size() == 1);
		CHECK(Components[0]->CachedDistance == 0.0f);
	}

	TEST_CASE("A reference to an object outside the scene is saved as null, with a warning")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		const FScopedTestScene OtherScene;
		WTestComponent* const Component = Scene->CreateEntity("Entity")->AddComponent<WTestComponent>();
		Component->TargetEntity = OtherScene->CreateEntity("Foreign");

		const FJsonValue Document = FSceneSerializer::SaveToJson(*Scene.Get());

		const FJsonValue& SavedComponent =
			Document.FindMember("Entities")->GetElements()[0].FindMember("Components")->GetElements()[0];
		const FJsonValue* const SavedReference = SavedComponent.FindMember("Properties")->FindMember("TargetEntity");
		REQUIRE(SavedReference != nullptr);
		CHECK(SavedReference->IsNull());
		REQUIRE(Capture.GetMessages().size() == 1);
		CHECK(Mentions(Capture.GetMessages()[0].Text, "TargetEntity"));
		CHECK(Mentions(Capture.GetMessages()[0].Text, "not part of the scene"));
	}

	TEST_CASE("Components are told they are attached after all their properties are loaded")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		Original->CreateEntity("First")->AddComponent<WTestComponent>()->Label = "One";
		Original->CreateEntity("Second")->AddComponent<WTestComponent>()->Label = "Two";
		const std::string Text = FSceneSerializer::SaveToText(*Original.Get());
		FTestComponentStats::Get().Events.clear();

		LoadClean(Text);

		// A component that heard it too early would report its default label.
		CHECK(FTestComponentStats::Get().Events == std::vector<std::string>{"Attached One", "Attached Two"});
	}

	TEST_CASE("The smallest scene document loads as an empty scene")
	{
		const FScopedSceneTest SceneTest;

		const WScene* const Scene = LoadClean(R"({"Format": "WhiteWolfScene", "Version": 1})");

		CHECK(Scene->GetName().empty());
		CHECK(Scene->GetAllEntities().empty());
	}

	TEST_CASE("Properties that a file leaves out keep their defaults")
	{
		const FScopedSceneTest SceneTest;

		const WScene* const Scene = LoadClean(SceneWithEntities(
			R"({"Id": 1, "Components": [{"Id": 2, "Class": "WTestComponent", "Properties": {"Health": 5}}]})"));

		const std::vector<WTestComponent*> Components = Scene->GetComponents<WTestComponent>();
		REQUIRE(Components.size() == 1);
		CHECK(Components[0]->Health == 5);
		CHECK(Components[0]->Label == "Unnamed");
		CHECK(Components[0]->GetOwner()->GetName().empty());
		CHECK(Components[0]->GetOwner()->GetRelativeTransform() == FTransform());
	}

	TEST_CASE("A child may come before its parent in the file, and siblings keep their order")
	{
		const FScopedSceneTest SceneTest;

		const WScene* const Scene =
			LoadClean(SceneWithEntities(R"({"Id": 3, "Parent": 1, "Properties": {"Name": "B"}},)"
										R"({"Id": 2, "Parent": 1, "Properties": {"Name": "A"}},)"
										R"({"Id": 1, "Properties": {"Name": "Root"}})"));

		REQUIRE(Scene->GetRootEntities().size() == 1);
		const WEntity* const Root = Scene->GetRootEntities()[0];
		CHECK(Root->GetName() == "Root");
		REQUIRE(Root->GetChildren().size() == 2);
		CHECK(Root->GetChildren()[0]->GetName() == "B");
		CHECK(Root->GetChildren()[1]->GetName() == "A");
	}

	TEST_CASE("A component of an unavailable class is kept and saved again unchanged")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const std::string Text = "{\n"
								 "  \"Format\": \"WhiteWolfScene\",\n"
								 "  \"Version\": 1,\n"
								 "  \"NextId\": 3,\n"
								 "  \"Properties\": {\n"
								 "    \"Name\": \"Physics\"\n"
								 "  },\n"
								 "  \"Entities\": [\n"
								 "    {\n"
								 "      \"Id\": 1,\n"
								 "      \"Properties\": {\n"
								 "        \"Name\": \"Crate\",\n"
								 "        \"RelativeTransform\": {\n"
								 "          \"Location\": [0.0, 0.0, 0.0],\n"
								 "          \"Rotation\": [1.0, 0.0, 0.0, 0.0],\n"
								 "          \"Scale\": [1.0, 1.0, 1.0]\n"
								 "        }\n"
								 "      },\n"
								 "      \"Components\": [\n"
								 "        {\n"
								 "          \"Id\": 2,\n"
								 "          \"Class\": \"WPluginRigidBodyComponent\",\n"
								 "          \"Properties\": {\n"
								 "            \"Mass\": 12.5,\n"
								 "            \"Shape\": {\n"
								 "              \"Kind\": \"Box\",\n"
								 "              \"Extent\": [1.0, 2.0, 3.0]\n"
								 "            }\n"
								 "          }\n"
								 "        }\n"
								 "      ]\n"
								 "    }\n"
								 "  ]\n"
								 "}\n";
		std::vector<std::string> Warnings;

		const WScene* const Scene = LoadValid(Text, Warnings);

		REQUIRE(Warnings.size() == 1);
		CHECK(Mentions(Warnings[0], "WPluginRigidBodyComponent"));
		CHECK(Mentions(Warnings[0], "not available"));
		const std::vector<WMissingComponent*> Missing = Scene->GetComponents<WMissingComponent>();
		REQUIRE(Missing.size() == 1);
		CHECK(Missing[0]->GetMissingClassName() == "WPluginRigidBodyComponent");
		CHECK(Missing[0]->GetId() == 2);
		REQUIRE(Missing[0]->GetSavedProperties().FindMember("Mass") != nullptr);
		CHECK(Missing[0]->GetSavedProperties().FindMember("Mass")->GetNumber() == 12.5);

		CHECK(FSceneSerializer::SaveToText(*Scene) == Text);
	}

	TEST_CASE("A component whose class is abstract or not a component class is kept in the same way")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		std::vector<std::string> Warnings;

		const WScene* const Scene =
			LoadValid(SceneWithEntities(R"({"Id": 1, "Components": [)"
										R"({"Id": 2, "Class": "WTestAbstractComponent"},)"
										R"({"Id": 3, "Class": "WEntity", "Properties": {"Name": "Odd"}}]})"),
				Warnings);

		REQUIRE(Warnings.size() == 2);
		CHECK(Mentions(Warnings[0], "is abstract"));
		CHECK(Mentions(Warnings[1], "is not a component class"));
		const std::vector<WMissingComponent*> Missing = Scene->GetComponents<WMissingComponent>();
		REQUIRE(Missing.size() == 2);
		CHECK(Missing[0]->GetMissingClassName() == "WTestAbstractComponent");
		CHECK(Missing[0]->GetSavedProperties() == FJsonValue::MakeObject());
		CHECK(Missing[1]->GetMissingClassName() == "WEntity");
	}

	TEST_CASE("A property that is unknown or has the wrong type is reported and the rest is loaded")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		std::vector<std::string> Warnings;

		const WScene* const Scene = LoadValid(
			SceneWithEntities(
				R"({"Id": 1, "Properties": {"Name": 7}, "Components": [{"Id": 2, "Class": "WTestComponent",)"
				R"("Properties": {"Helth": 1, "Health": "full", "Label": "Loaded", "CachedDistance": 2.0}}]})"),
			Warnings);

		REQUIRE(Warnings.size() == 4);
		CHECK(Warnings[0] == "entity 1: property \"Name\": expected a string but found a number");
		CHECK(Warnings[1] ==
			"component 2 (WTestComponent) of entity 1: there is no property named \"Helth\"; the value was ignored");
		CHECK(Warnings[2] ==
			"component 2 (WTestComponent) of entity 1: property \"Health\": expected a number but found a string");
		CHECK(Mentions(Warnings[3], "\"CachedDistance\" is not saved with a scene"));
		CHECK(Capture.GetMessages().size() == 4);

		const std::vector<WTestComponent*> Components = Scene->GetComponents<WTestComponent>();
		REQUIRE(Components.size() == 1);
		CHECK(Components[0]->Health == 100);
		CHECK(Components[0]->Label == "Loaded");
		CHECK(Components[0]->CachedDistance == 0.0f);
	}

	TEST_CASE("A reference that cannot be resolved is reported and left null")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		std::vector<std::string> Warnings;

		const WScene* const Scene = LoadValid(
			SceneWithEntities(R"({"Id": 1, "Components": [{"Id": 2, "Class": "WTestComponent", "Properties": {)"
							  R"("TargetEntity": 99, "Partner": 1, "Waypoints": [1, true]}}]})"),
			Warnings);

		REQUIRE(Warnings.size() == 3);
		CHECK(Mentions(Warnings[0], "\"TargetEntity\": there is no entity or component with the identifier 99"));
		CHECK(Mentions(Warnings[1], "\"Partner\": the reference is to a WEntity, which is not a WTestComponent"));
		// A string would be the path of an asset; a boolean is no kind of reference.
		CHECK(Mentions(Warnings[2],
			"\"Waypoints\": element 1: expected the identifier of an entity or component, or the path of an asset, "
			"but found a boolean"));

		const std::vector<WTestComponent*> Components = Scene->GetComponents<WTestComponent>();
		REQUIRE(Components.size() == 1);
		CHECK(Components[0]->TargetEntity.Get() == nullptr);
		CHECK(Components[0]->Partner.Get() == nullptr);
	}

	TEST_CASE("Unknown keys are reported, because they are usually typing mistakes")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		std::vector<std::string> Warnings;

		LoadValid(R"({"Format": "WhiteWolfScene", "Version": 1, "Entitys": [], "Entities": [)"
				  R"({"Id": 1, "Parnet": 2, "Components": [{"Id": 2, "Class": "WTestComponent", "Props": {}}]}]})",
			Warnings);

		REQUIRE(Warnings.size() == 3);
		CHECK(Mentions(Warnings[0], "component 2 of entity 1 has the unknown key \"Props\""));
		CHECK(Mentions(Warnings[1], "entity 1 has the unknown key \"Parnet\""));
		CHECK(Mentions(Warnings[2], "the scene has the unknown key \"Entitys\""));
	}

	TEST_CASE("Text that is not JSON is refused with the place of the error")
	{
		const FScopedSceneTest SceneTest;

		const std::string Error = LoadInvalid("{\n  \"Format\": \"WhiteWolfScene\",\n  \"Version\": 1,\n}");

		CHECK(Mentions(Error, "not valid JSON"));
		CHECK(Mentions(Error, "line 4"));
	}

	TEST_CASE("A document that is not a scene of a readable version is refused")
	{
		const FScopedSceneTest SceneTest;

		CHECK(Mentions(LoadInvalid("[]"), "must be a JSON object"));
		CHECK(Mentions(LoadInvalid("{}"), "\"Format\" must be \"WhiteWolfScene\""));
		CHECK(Mentions(LoadInvalid(R"({"Format": "Prefab", "Version": 1})"), "\"Format\" must be \"WhiteWolfScene\""));
		CHECK(Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene"})"), "\"Version\" must be a positive whole number"));
		CHECK(Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": "1"})"), "\"Version\" must be"));
		CHECK(Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": 0})"), "\"Version\" must be"));
		CHECK(Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": 2})"),
			"format version 2, but this build reads only up to version 1"));
	}

	TEST_CASE("A document with a malformed top-level member is refused")
	{
		const FScopedSceneTest SceneTest;

		CHECK(
			Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": 1, "NextId": 0})"), "\"NextId\" must be"));
		CHECK(Mentions(
			LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": 1, "NextId": "5"})"), "\"NextId\" must be"));
		CHECK(Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": 1, "Properties": []})"),
			"\"Properties\" of the scene must be an object"));
		CHECK(Mentions(LoadInvalid(R"({"Format": "WhiteWolfScene", "Version": 1, "Entities": {}})"),
			"\"Entities\" must be an array"));
	}

	TEST_CASE("An entity without a usable identifier is refused")
	{
		const FScopedSceneTest SceneTest;

		CHECK(Mentions(LoadInvalid(SceneWithEntities("7")), "element 0 of \"Entities\" must be an object"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities("{}")), "needs an \"Id\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 0})")), "needs an \"Id\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": -4})")), "needs an \"Id\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": "1"})")), "needs an \"Id\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1.5})")), "needs an \"Id\""));
	}

	TEST_CASE("An identifier used twice is refused, whether by entities or components")
	{
		const FScopedSceneTest SceneTest;

		CHECK(Mentions(
			LoadInvalid(SceneWithEntities(R"({"Id": 1}, {"Id": 1})")), "identifier 1 is used by more than one"));
		CHECK(Mentions(
			LoadInvalid(SceneWithEntities(R"({"Id": 1, "Components": [{"Id": 1, "Class": "WTestComponent"}]})")),
			"identifier 1 is used by more than one"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(
						   R"({"Id": 1, "Components": [{"Id": 2, "Class": "WTestComponent"}]}, {"Id": 2})")),
			"identifier 2 is used by more than one"));
	}

	TEST_CASE("A parent that is malformed, missing or a descendant is refused")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;

		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Parent": "Root"})")),
			"\"Parent\" of entity 1 must be the identifier"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Parent": 5})")),
			"entity 1 has the parent 5, which is not an entity"));
		CHECK(
			Mentions(LoadInvalid(SceneWithEntities(
						 R"({"Id": 1, "Components": [{"Id": 2, "Class": "WTestComponent"}]}, {"Id": 3, "Parent": 2})")),
				"entity 3 has the parent 2, which is not an entity"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Parent": 1})")), "cannot be its own ancestor"));
		CHECK(Mentions(
			LoadInvalid(SceneWithEntities(R"({"Id": 1, "Parent": 2}, {"Id": 2, "Parent": 3}, {"Id": 3, "Parent": 1})")),
			"cannot be its own ancestor"));
	}

	TEST_CASE("A null parent is the same as no parent")
	{
		const FScopedSceneTest SceneTest;

		const WScene* const Scene = LoadClean(SceneWithEntities(R"({"Id": 1, "Parent": null})"));

		CHECK(Scene->GetRootEntities().size() == 1);
	}

	TEST_CASE("Malformed properties and components are refused")
	{
		const FScopedSceneTest SceneTest;

		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Properties": "Name"})")),
			"\"Properties\" of entity 1 must be an object"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Components": {}})")),
			"\"Components\" of entity 1 must be an array"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Components": ["WTestComponent"]})")),
			"every element of \"Components\" of entity 1 must be an object"));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Components": [{"Class": "WTestComponent"}]})")),
			"a component of entity 1 needs an \"Id\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Components": [{"Id": 2}]})")),
			"component 2 of entity 1 needs a \"Class\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(R"({"Id": 1, "Components": [{"Id": 2, "Class": ""}]})")),
			"component 2 of entity 1 needs a \"Class\""));
		CHECK(Mentions(LoadInvalid(SceneWithEntities(
						   R"({"Id": 1, "Components": [{"Id": 2, "Class": "WTestComponent", "Properties": 3}]})")),
			"\"Properties\" of component 2 of entity 1 must be an object"));
	}

	TEST_CASE("A scene is saved to a file and loaded from it")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Scenes" / "Sample.scene.json";
		const FScopedTestScene Original;
		FillSampleScene(*Original.Get());
		std::string Error;

		REQUIRE(FSceneSerializer::SaveToFile(*Original.Get(), Path, Error));
		std::string FileText;
		REQUIRE(FFileHelper::LoadFileToString(Path, FileText, Error));
		const WScene* const Loaded = FSceneSerializer::LoadFromFile(Path, Error);

		CHECK(FileText == FSceneSerializer::SaveToText(*Original.Get()));
		REQUIRE(Loaded != nullptr);
		CHECK(FSceneSerializer::SaveToText(*Loaded) == FileText);
	}

	TEST_CASE("Loading a file that is missing or not a scene fails and names the file")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Missing = Directory.GetPath() / "Missing.scene.json";
		const std::filesystem::path NotAScene = Directory.GetPath() / "Other.json";
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile("{\"Format\": \"Prefab\"}", NotAScene, Error));

		CHECK(FSceneSerializer::LoadFromFile(Missing, Error) == nullptr);
		CHECK(Mentions(Error, "Missing.scene.json"));

		CHECK(FSceneSerializer::LoadFromFile(NotAScene, Error) == nullptr);
		CHECK(Mentions(Error, "Other.json"));
		CHECK(Mentions(Error, "this is not a scene"));
	}

	TEST_CASE("A loaded scene belongs to the caller: unreferenced, it is collected")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Original;
		FillSampleScene(*Original.Get());
		const std::string Text = FSceneSerializer::SaveToText(*Original.Get());
		const int32 ObjectCountBefore = FGarbageCollector::GetObjectCount();

		LoadClean(Text);
		CHECK(FGarbageCollector::GetObjectCount() > ObjectCountBefore);

		CollectGarbage();
		CHECK(FGarbageCollector::GetObjectCount() == ObjectCountBefore);
	}
}
