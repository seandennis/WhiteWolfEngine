#include "Assets/TestAssets.h"
#include "Editor.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** An editor whose scene has an entity "Player" with a derived test component, and an entity "Target". */
	struct FPropertyCommandTest
	{
		FPropertyCommandTest()
		{
			Player = Editor.GetScene().CreateEntity("Player");
			REQUIRE(Player != nullptr);
			Target = Editor.GetScene().CreateEntity("Target");
			REQUIRE(Target != nullptr);
			Component = Player->AddComponent<WEditorTestDerivedComponent>();
			REQUIRE(Component != nullptr);
		}

		/** Gives the editor a command with the arguments written as JSON text. */
		FCommandResult Execute(std::string_view CommandName, const std::string& ArgumentText)
		{
			return Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
		}

		/** Sets a property of the component to the value written as JSON text. */
		FCommandResult SetComponentProperty(std::string_view PropertyName, std::string_view ValueText)
		{
			return Execute("Object.SetProperty",
				std::format(
					R"({{"Object": {}, "Property": "{}", "Value": {}}})", Component->GetId(), PropertyName, ValueText));
		}

		/** Returns a property of the component as JSON text, or the error if it cannot be read. */
		std::string GetComponentProperty(std::string_view PropertyName)
		{
			const FCommandResult Result = Execute("Object.GetProperty",
				std::format(R"({{"Object": {}, "Property": "{}"}})", Component->GetId(), PropertyName));
			return Result.HasSucceeded() ? WriteTestJson(Result.GetValue()) : Result.GetError();
		}

		FScopedEditorTest EditorTest;
		FEditor Editor;

		// The scene, which the editor keeps alive, keeps these alive.
		WEntity* Player = nullptr;
		WEntity* Target = nullptr;
		WEditorTestDerivedComponent* Component = nullptr;
	};
}

TEST_SUITE("Property commands")
{
	TEST_CASE("Object.GetProperties returns the class of a component and its saved properties, inherited ones first")
	{
		FPropertyCommandTest Test;
		Test.Component->Health = 80;
		Test.Component->TargetEntity = Test.Target;
		Test.Component->Scores = {3, 5};
		Test.Component->CachedDistance = 12.5f;

		const FCommandResult Result =
			Test.Execute("Object.GetProperties", std::format(R"({{"Object": {}}})", Test.Component->GetId()));

		REQUIRE(Result.HasSucceeded());
		// CachedDistance is Transient, and so is the component's owner: neither is listed.
		const std::string Expected =
			std::format(R"({{"Id":{},"Class":"WEditorTestDerivedComponent","Properties":)"
						R"({{"Health":80,"Label":"Unnamed","TargetEntity":{},"Scores":[3,5],"Range":1.5}}}})",
				Test.Component->GetId(),
				Test.Target->GetId());
		CHECK(WriteTestJson(Result.GetValue()) == Expected);
	}

	TEST_CASE("Object.GetProperties returns the name and transform of an entity")
	{
		FPropertyCommandTest Test;

		const FCommandResult Result =
			Test.Execute("Object.GetProperties", std::format(R"({{"Object": {}}})", Test.Player->GetId()));

		REQUIRE(Result.HasSucceeded());
		const FJsonValue& Value = Result.GetValue();
		REQUIRE(Value.FindMember("Id") != nullptr);
		CHECK(*Value.FindMember("Id") == FJsonValue(Test.Player->GetId()));
		REQUIRE(Value.FindMember("Class") != nullptr);
		CHECK(*Value.FindMember("Class") == FJsonValue("WEntity"));
		const FJsonValue* const Properties = Value.FindMember("Properties");
		REQUIRE(Properties != nullptr);
		// The hierarchy is not among them: it is Transient, and has commands of its own.
		REQUIRE(Properties->GetMembers().size() == 2);
		CHECK(Properties->GetMembers()[0].first == "Name");
		CHECK(Properties->GetMembers()[0].second == FJsonValue("Player"));
		CHECK(Properties->GetMembers()[1].first == "RelativeTransform");
	}

	TEST_CASE("Object.GetProperty returns the value of one property")
	{
		FPropertyCommandTest Test;
		Test.Component->Health = 80;
		Test.Component->Label = "Hero";
		Test.Component->Scores = {3, 5};

		CHECK(Test.GetComponentProperty("Health") == "80");
		CHECK(Test.GetComponentProperty("Label") == "\"Hero\"");
		CHECK(Test.GetComponentProperty("Scores") == "[3,5]");
		CHECK(Test.GetComponentProperty("Range") == "1.5");
	}

	TEST_CASE("A reference to an entity is read as its identifier, and no reference as null")
	{
		FPropertyCommandTest Test;
		CHECK(Test.GetComponentProperty("TargetEntity") == "null");

		Test.Component->TargetEntity = Test.Target;

		CHECK(Test.GetComponentProperty("TargetEntity") == std::format("{}", Test.Target->GetId()));
	}

	TEST_CASE("Object.SetProperty changes the value of a property")
	{
		FPropertyCommandTest Test;

		REQUIRE(Test.SetComponentProperty("Health", "25").HasSucceeded());
		REQUIRE(Test.SetComponentProperty("Label", "\"Hero\"").HasSucceeded());
		REQUIRE(Test.SetComponentProperty("Scores", "[1, 2, 3]").HasSucceeded());
		REQUIRE(Test.SetComponentProperty("Range", "7.25").HasSucceeded());

		CHECK(Test.Component->Health == 25);
		CHECK(Test.Component->Label == "Hero");
		CHECK(Test.Component->Scores == std::vector<int32>{1, 2, 3});
		CHECK(Test.Component->Range == 7.25);
	}

	TEST_CASE("Object.SetProperty sets a reference from an identifier and clears it with null")
	{
		FPropertyCommandTest Test;

		REQUIRE(Test.SetComponentProperty("TargetEntity", std::format("{}", Test.Target->GetId())).HasSucceeded());
		CHECK(Test.Component->TargetEntity.Get() == Test.Target);

		REQUIRE(Test.SetComponentProperty("TargetEntity", "null").HasSucceeded());
		CHECK(Test.Component->TargetEntity.Get() == nullptr);
	}

	TEST_CASE("A reference to an asset is set from the asset's path and read as that path")
	{
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		FPropertyCommandTest Test;
		const WStaticMeshComponent* const MeshComponent = Test.Player->AddComponent<WStaticMeshComponent>();
		REQUIRE(MeshComponent != nullptr);

		const FCommandResult Written = Test.Execute("Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Material", "Value": "Models/Square.gltf#Materials/0"}})",
				MeshComponent->GetId()));

		INFO("Error: ", Written.GetError());
		REQUIRE(Written.HasSucceeded());
		REQUIRE(MeshComponent->Material != nullptr);
		CHECK(MeshComponent->Material->GetAssetPath() == "Models/Square.gltf#Materials/0");

		const FCommandResult Read = Test.Execute(
			"Object.GetProperty", std::format(R"({{"Object": {}, "Property": "Material"}})", MeshComponent->GetId()));
		REQUIRE(Read.HasSucceeded());
		CHECK(Read.GetValue() == FJsonValue("Models/Square.gltf#Materials/0"));
	}

	TEST_CASE("An asset that cannot be loaded, or is of another class, is refused and the reference left as it was")
	{
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		FPropertyCommandTest Test;
		const WStaticMeshComponent* const MeshComponent = Test.Player->AddComponent<WStaticMeshComponent>();
		REQUIRE(MeshComponent != nullptr);
		std::string AssetPath;
		std::string ExpectedReason;

		SUBCASE("A file that is not there")
		{
			AssetPath = "Models/Gone.gltf#Materials/0";
			ExpectedReason = "the content directory has no file \"Models/Gone.gltf\"";
		}
		SUBCASE("A mesh where a material is wanted")
		{
			AssetPath = "Models/Square.gltf#Meshes/0/Primitives/0";
			ExpectedReason = "the reference is to a WStaticMesh, which is not a WMaterial";
		}

		const FCommandResult Result = Test.Execute("Object.SetProperty",
			std::format(
				R"({{"Object": {}, "Property": "Material", "Value": "{}"}})", MeshComponent->GetId(), AssetPath));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			"The property \"Material\" of the class WStaticMeshComponent was not changed: " + ExpectedReason + ".");
		CHECK(MeshComponent->Material == nullptr);
	}

	TEST_CASE("Object.SetProperty changes the properties of an entity")
	{
		FPropertyCommandTest Test;

		const FCommandResult Result = Test.Execute("Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Name", "Value": "Hero"}})", Test.Player->GetId()));

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK(Test.Player->GetName() == "Hero");
	}

	TEST_CASE("A value read with Object.GetProperty can be given back to Object.SetProperty")
	{
		FPropertyCommandTest Test;
		Test.Player->SetWorldLocation(FVector(1.0e13, -2.5, 3.0));
		const FCommandResult Read = Test.Execute("Object.GetProperty",
			std::format(R"({{"Object": {}, "Property": "RelativeTransform"}})", Test.Player->GetId()));
		REQUIRE(Read.HasSucceeded());

		const FCommandResult Written = Test.Execute("Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "RelativeTransform", "Value": {}}})",
				Test.Target->GetId(),
				WriteTestJson(Read.GetValue())));

		REQUIRE(Written.HasSucceeded());
		// Double precision survives: ten billion kilometres and a fraction of a metre.
		CHECK(Test.Target->GetWorldLocation() == FVector(1.0e13, -2.5, 3.0));
	}

	TEST_CASE("Object.SetProperty refuses a value the property cannot take, and leaves the property as it was")
	{
		FPropertyCommandTest Test;
		Test.Component->TargetEntity = Test.Player;
		std::string PropertyName;
		std::string ValueText;

		SUBCASE("A string for a number")
		{
			PropertyName = "Health";
			ValueText = "\"full\"";
		}
		SUBCASE("A number too large for the property")
		{
			PropertyName = "Health";
			ValueText = "3000000000";
		}
		SUBCASE("A number for a string")
		{
			PropertyName = "Label";
			ValueText = "5";
		}
		SUBCASE("A reference to something that does not exist")
		{
			PropertyName = "TargetEntity";
			ValueText = "99";
		}
		SUBCASE("A reference to an object of the wrong class")
		{
			PropertyName = "TargetEntity";
			ValueText = std::format("{}", Test.Component->GetId());
		}
		SUBCASE("A reference that is not an identifier")
		{
			PropertyName = "TargetEntity";
			ValueText = "\"Target\"";
		}

		const FCommandResult Result = Test.SetComponentProperty(PropertyName, ValueText);

		CHECK_FALSE(Result.HasSucceeded());
		const std::string ExpectedStart =
			std::format("The property \"{}\" of the class WEditorTestDerivedComponent was not changed: ", PropertyName);
		CHECK(Result.GetError().starts_with(ExpectedStart));
		CHECK(Result.GetError().size() > ExpectedStart.size() + 1);
		CHECK(Result.GetError().ends_with("."));
		CHECK(Test.Component->Health == 100);
		CHECK(Test.Component->Label == "Unnamed");
		CHECK(Test.Component->TargetEntity.Get() == Test.Player);
	}

	TEST_CASE("Properties that are not saved with the scene are neither read nor changed")
	{
		FPropertyCommandTest Test;
		Test.Component->CachedDistance = 12.5f;
		const std::string ExpectedError =
			"The property \"CachedDistance\" of the class WEditorTestDerivedComponent is not saved with the scene, and "
			"commands neither read nor change such properties.";

		CHECK(Test.GetComponentProperty("CachedDistance") == ExpectedError);

		const FCommandResult Result = Test.SetComponentProperty("CachedDistance", "1.0");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Test.Component->CachedDistance == 12.5f);
	}

	TEST_CASE("The hierarchy cannot be changed through properties")
	{
		FPropertyCommandTest Test;

		const FCommandResult Result = Test.Execute("Object.SetProperty",
			std::format(
				R"({{"Object": {}, "Property": "Parent", "Value": {}}})", Test.Player->GetId(), Test.Target->GetId()));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Test.Player->GetParent() == nullptr);
	}

	TEST_CASE("A property that the class does not have cannot be read or changed")
	{
		FPropertyCommandTest Test;
		const std::string ExpectedError = "The class WEditorTestDerivedComponent has no property \"Mana\".";

		CHECK(Test.GetComponentProperty("Mana") == ExpectedError);

		const FCommandResult Result = Test.SetComponentProperty("Mana", "5");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
	}

	TEST_CASE("The property commands fail for an object that does not exist")
	{
		FPropertyCommandTest Test;
		const std::string ExpectedError = "There is no entity or component with the identifier 99.";

		const FCommandResult All = Test.Execute("Object.GetProperties", R"({"Object": 99})");
		CHECK_FALSE(All.HasSucceeded());
		CHECK(All.GetError() == ExpectedError);

		const FCommandResult Read = Test.Execute("Object.GetProperty", R"({"Object": 99, "Property": "Health"})");
		CHECK_FALSE(Read.HasSucceeded());
		CHECK(Read.GetError() == ExpectedError);

		const FCommandResult Written =
			Test.Execute("Object.SetProperty", R"({"Object": 99, "Property": "Health", "Value": 1})");
		CHECK_FALSE(Written.HasSucceeded());
		CHECK(Written.GetError() == ExpectedError);
	}
}
