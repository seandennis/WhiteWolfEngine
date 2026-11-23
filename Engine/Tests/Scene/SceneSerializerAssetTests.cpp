#include "Assets/AssetManager.h"
#include "Assets/TestAssets.h"
#include "Json/JsonValue.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/Material.h"
#include "Rendering/SkyLightComponent.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Returns the text of a scene with one entity that has a static mesh component with the given properties. */
	std::string MakeSceneWithMeshComponent(std::string_view Properties)
	{
		return std::string(R"({
			"Format": "WhiteWolfScene",
			"Version": 1,
			"NextId": 3,
			"Properties": { "Name": "Level" },
			"Entities": [
				{
					"Id": 1,
					"Properties": { "Name": "Square" },
					"Components": [ { "Id": 2, "Class": "WStaticMeshComponent", "Properties": )") +
			std::string(Properties) + R"( } ]
				}
			]
		})";
	}

	bool HasWarning(const std::vector<std::string>& Warnings, std::string_view Text)
	{
		for (const std::string& Warning : Warnings)
		{
			if (Warning.find(Text) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}
}

TEST_SUITE("FSceneSerializer asset references")
{
	TEST_CASE("A reference to an asset of the content directory is saved as the asset's path")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		Content.WriteEnvironment("Skies/Evening.hdr");
		const FScopedTestScene Scene;
		std::string Error;
		WStaticMeshComponent* const MeshComponent = Scene->CreateEntity("Square")->AddComponent<WStaticMeshComponent>();
		MeshComponent->Mesh = FAssetManager::Load<WStaticMesh>("Models/Square.gltf#Meshes/0/Primitives/0", Error);
		MeshComponent->Material = FAssetManager::Load<WMaterial>("Models/Square.gltf#Materials/0", Error);
		WSkyLightComponent* const SkyLight = Scene->CreateEntity("Sky")->AddComponent<WSkyLightComponent>();
		SkyLight->Environment = FAssetManager::Load<WEnvironmentMap>("Skies/Evening.hdr", Error);
		REQUIRE(MeshComponent->Mesh != nullptr);
		REQUIRE(MeshComponent->Material != nullptr);
		REQUIRE(SkyLight->Environment != nullptr);

		const FJsonValue Document = FSceneSerializer::SaveToJson(*Scene.Get());

		const FJsonValue::FArray& Entities = Document.FindMember("Entities")->GetElements();
		const FJsonValue* const MeshProperties =
			Entities[0].FindMember("Components")->GetElements()[0].FindMember("Properties");
		CHECK(*MeshProperties->FindMember("Mesh") == FJsonValue("Models/Square.gltf#Meshes/0/Primitives/0"));
		CHECK(*MeshProperties->FindMember("Material") == FJsonValue("Models/Square.gltf#Materials/0"));
		const FJsonValue* const SkyProperties =
			Entities[1].FindMember("Components")->GetElements()[0].FindMember("Properties");
		CHECK(*SkyProperties->FindMember("Environment") == FJsonValue("Skies/Evening.hdr"));
		CHECK(CountLoggedProblems(Capture) == 0);
	}

	TEST_CASE("Loading a scene loads the assets it refers to")
	{
		const FScopedSceneTest SceneTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		std::string Error;
		std::vector<std::string> Warnings;

		const WScene* const Scene = FSceneSerializer::LoadFromText(
			MakeSceneWithMeshComponent(
				R"({ "Mesh": "Models/Square.gltf#Meshes/0/Primitives/0", "Material": "Models/Square.gltf#Materials/0" })"),
			Error,
			&Warnings);

		INFO("Error: ", Error);
		REQUIRE(Scene != nullptr);
		CHECK(Warnings.empty());
		const std::vector<WStaticMeshComponent*> Components = Scene->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetAssetPath() == "Models/Square.gltf#Meshes/0/Primitives/0");
		CHECK(Components[0]->Mesh->GetTriangleCount() == 2);
		REQUIRE(Components[0]->Material != nullptr);
		CHECK(Components[0]->Material->GetAssetPath() == "Models/Square.gltf#Materials/0");
	}

	TEST_CASE("A built-in shape is saved as its path and loaded again, without a content directory")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		REQUIRE(FAssetManager::GetContentDirectory().empty());
		std::string Error;
		std::string Text;
		{
			const FScopedTestScene Scene;
			WStaticMeshComponent* const Component = Scene->CreateEntity("Crate")->AddComponent<WStaticMeshComponent>();
			Component->Mesh = FAssetManager::Load<WStaticMesh>(FAssetManager::CubeShape, Error);
			REQUIRE(Component->Mesh != nullptr);

			Text = FSceneSerializer::SaveToText(*Scene.Get());
		}
		CHECK(Text.find("\"Mesh\": \"Engine/Shapes#Cube\"") != std::string::npos);

		std::vector<std::string> Warnings;
		const WScene* const Loaded = FSceneSerializer::LoadFromText(Text, Error, &Warnings);

		INFO("Error: ", Error);
		REQUIRE(Loaded != nullptr);
		CHECK(Warnings.empty());
		const std::vector<WStaticMeshComponent*> Components = Loaded->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetAssetPath() == "Engine/Shapes#Cube");
		CHECK(Components[0]->Mesh->HasGeometry());
		CHECK(FSceneSerializer::SaveToText(*Loaded) == Text);
		CHECK(CountLoggedProblems(Capture) == 0);
	}

	TEST_CASE("A scene with asset references is the same text after it has been saved, loaded and saved again")
	{
		const FScopedSceneTest SceneTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		Content.WritePicture("Textures/Wood.png");
		const FScopedTestScene Scene;
		std::string Error;
		for (const char* const Name : {"First", "Second"})
		{
			// Two components that share a mesh and a material.
			WStaticMeshComponent* const Component = Scene->CreateEntity(Name)->AddComponent<WStaticMeshComponent>();
			Component->Mesh = FAssetManager::Load<WStaticMesh>("Models/Square.gltf#Meshes/0/Primitives/0", Error);
			Component->Material = FAssetManager::Load<WMaterial>("Models/Square.gltf#Materials/0", Error);
		}
		const std::string Saved = FSceneSerializer::SaveToText(*Scene.Get());

		const WScene* const Loaded = FSceneSerializer::LoadFromText(Saved, Error);

		REQUIRE(Loaded != nullptr);
		CHECK(FSceneSerializer::SaveToText(*Loaded) == Saved);
		const std::vector<WStaticMeshComponent*> Components = Loaded->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 2);
		// What was shared is shared again, and with the scene it was saved from.
		CHECK(Components[0]->Mesh.Get() == Components[1]->Mesh.Get());
		CHECK(Components[0]->Mesh.Get() == Scene->GetComponents<WStaticMeshComponent>()[0]->Mesh.Get());
	}

	TEST_CASE("A reference to an asset that cannot be loaded is a warning, and the rest of the scene loads")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		std::string Properties;
		std::string ExpectedWarning;

		SUBCASE("A file that is not there")
		{
			Properties = R"({ "Mesh": "Models/Gone.gltf#Meshes/0/Primitives/0", "bCastShadows": false })";
			ExpectedWarning = "the content directory has no file \"Models/Gone.gltf\"";
		}
		SUBCASE("An asset the file does not have")
		{
			Properties = R"({ "Mesh": "Models/Square.gltf#Meshes/3/Primitives/0", "bCastShadows": false })";
			ExpectedWarning = "the file \"Models/Square.gltf\" has no asset \"Meshes/3/Primitives/0\"";
		}
		SUBCASE("An asset of another class")
		{
			Properties = R"({ "Mesh": "Models/Square.gltf#Materials/0", "bCastShadows": false })";
			ExpectedWarning = "the reference is to a WMaterial, which is not a WStaticMesh";
		}
		SUBCASE("Text that is not an asset path")
		{
			Properties = R"({ "Mesh": "../Square.gltf", "bCastShadows": false })";
			ExpectedWarning = "is not the path of an asset";
		}

		std::string Error;
		std::vector<std::string> Warnings;
		const WScene* const Scene =
			FSceneSerializer::LoadFromText(MakeSceneWithMeshComponent(Properties), Error, &Warnings);

		REQUIRE(Scene != nullptr);
		INFO("Warnings: ", Warnings.empty() ? std::string() : Warnings[0]);
		REQUIRE(Warnings.size() == 1);
		CHECK(HasWarning(Warnings, "\"Mesh\""));
		CHECK(HasWarning(Warnings, ExpectedWarning));
		const std::vector<WStaticMeshComponent*> Components = Scene->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		CHECK(Components[0]->Mesh == nullptr);
		CHECK_FALSE(Components[0]->bCastShadows);
	}

	TEST_CASE("A reference to an asset that was made in code is saved as null, with a warning")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		WStaticMeshComponent* const Component = Scene->CreateEntity("Square")->AddComponent<WStaticMeshComponent>();
		Component->Material = NewObject<WMaterial>();

		const FJsonValue Document = FSceneSerializer::SaveToJson(*Scene.Get());

		const FJsonValue* const Properties = Document.FindMember("Entities")
												 ->GetElements()[0]
												 .FindMember("Components")
												 ->GetElements()[0]
												 .FindMember("Properties");
		CHECK(Properties->FindMember("Material")->IsNull());
		REQUIRE(Capture.GetMessages().size() == 1);
		CHECK(Capture.GetMessages()[0].Text.find("has no asset path") != std::string::npos);
	}
}
