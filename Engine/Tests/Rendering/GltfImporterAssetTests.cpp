#include "Assets/AssetManager.h"
#include "Assets/AssetPackage.h"
#include "Assets/TestAssets.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/GltfImporter.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestGltfFiles.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

// A glTF file that is in the content directory is imported as the assets of its package. The import of files
// anywhere else, which get meshes and materials of their own, is tested in GltfImporterTests.cpp.

TEST_SUITE("FGltfImporter in the content directory")
{
	TEST_CASE("The components of an imported file refer to the assets of the file's package")
	{
		const FScopedSceneTest SceneTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		const FScopedTestScene Scene;
		std::string Error;
		std::vector<std::string> Warnings;

		const WEntity* const Root =
			FGltfImporter::Import(Content.GetPath() / "Models" / "Square.gltf", *Scene.Get(), Error, &Warnings);

		INFO("Error: ", Error);
		REQUIRE(Root != nullptr);
		CHECK(Warnings.empty());
		const std::vector<WStaticMeshComponent*> Components = Scene->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Mesh != nullptr);
		REQUIRE(Components[0]->Material != nullptr);
		CHECK(Components[0]->Mesh->GetAssetPath() == "Models/Square.gltf#Meshes/0/Primitives/0");
		CHECK(Components[0]->Material->GetAssetPath() == "Models/Square.gltf#Materials/0");
		CHECK(Components[0]->Mesh.Get() == FAssetManager::Load("Models/Square.gltf#Meshes/0/Primitives/0", Error));
	}

	TEST_CASE("A scene with an imported file can be saved and loaded with its meshes and materials")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Square.gltf");
		const FScopedTestScene Scene;
		std::string Error;
		REQUIRE(FGltfImporter::Import(Content.GetPath() / "Square.gltf", *Scene.Get(), Error) != nullptr);

		const std::string Saved = FSceneSerializer::SaveToText(*Scene.Get());
		const WScene* const Loaded = FSceneSerializer::LoadFromText(Saved, Error);

		REQUIRE(Loaded != nullptr);
		// Nothing was saved as null: no warning.
		CHECK(CountLoggedProblems(Capture) == 0);
		const std::vector<WStaticMeshComponent*> Components = Loaded->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetTriangleCount() == 2);
		REQUIRE(Components[0]->Material != nullptr);
		CHECK(FSceneSerializer::SaveToText(*Loaded) == Saved);
	}

	TEST_CASE("A file that is imported twice shares its meshes and materials")
	{
		const FScopedSceneTest SceneTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Square.gltf");
		const FScopedTestScene Scene;
		std::string Error;

		REQUIRE(FGltfImporter::Import(Content.GetPath() / "Square.gltf", *Scene.Get(), Error) != nullptr);
		REQUIRE(FGltfImporter::Import(Content.GetPath() / "Square.gltf", *Scene.Get(), Error) != nullptr);

		const std::vector<WStaticMeshComponent*> Components = Scene->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 2);
		CHECK(Components[0]->Mesh.Get() == Components[1]->Mesh.Get());
		CHECK(Components[0]->Material.Get() == Components[1]->Material.Get());
	}

	TEST_CASE("What the file leaves out is reported by every import, also when the file is loaded already")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedContentDirectory Content;
		FTestGltf Gltf;
		Gltf.Materials = R"([{"name": "Glass", "alphaMode": "BLEND"}])";
		Gltf.More = R"("cameras": [{"type": "perspective", "perspective": {"yfov": 1.0, "znear": 0.1}}], )";
		Content.WriteText("Glass.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		const FScopedTestScene Scene;
		std::string Error;

		for (int32 Time = 0; Time < 2; ++Time)
		{
			std::vector<std::string> Warnings;

			REQUIRE(FGltfImporter::Import(Content.GetPath() / "Glass.gltf", *Scene.Get(), Error, &Warnings) != nullptr);

			// One warning from the file's assets and one from its scene.
			CAPTURE(Time);
			REQUIRE(Warnings.size() == 2);
			CHECK(Warnings[0].find("transparent") != std::string::npos);
			CHECK(Warnings[1].find("cameras") != std::string::npos);
		}
	}

	TEST_CASE("A file of the content directory that cannot be imported leaves the scene as it was")
	{
		const FScopedSceneTest SceneTest;
		const FScopedContentDirectory Content;
		Content.WriteText("Broken.gltf", "{ this is not glTF");
		const FScopedTestScene Scene;
		std::string Error;

		CHECK(FGltfImporter::Import(Content.GetPath() / "Broken.gltf", *Scene.Get(), Error) == nullptr);

		CHECK(Error.find("Broken.gltf") != std::string::npos);
		CHECK(Scene->GetAllEntities().empty());
	}

	TEST_CASE("A file outside the content directory gets meshes and materials of its own, without asset paths")
	{
		const FScopedSceneTest SceneTest;
		const FScopedContentDirectory Content;
		const FScopedTempDirectory Elsewhere;
		WriteTestGltfFile(
			Elsewhere.GetPath() / "Square.gltf", FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		const FScopedTestScene Scene;
		std::string Error;

		REQUIRE(FGltfImporter::Import(Elsewhere.GetPath() / "Square.gltf", *Scene.Get(), Error) != nullptr);
		REQUIRE(FGltfImporter::Import(Elsewhere.GetPath() / "Square.gltf", *Scene.Get(), Error) != nullptr);

		const std::vector<WStaticMeshComponent*> Components = Scene->GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 2);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetAssetPath().empty());
		CHECK(Components[0]->Mesh->GetPackage() == nullptr);
		CHECK(Components[0]->Mesh.Get() != Components[1]->Mesh.Get());
	}

	TEST_CASE("LoadAssets returns the assets of a file with their keys")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTempDirectory Directory;
		WriteTestGltfFile(
			Directory.GetPath() / "Square.gltf", FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		std::vector<std::pair<std::string, WAsset*>> Assets;
		std::string Error;

		REQUIRE(FGltfImporter::LoadAssets(Directory.GetPath() / "Square.gltf", Assets, Error));

		REQUIRE(Assets.size() == 2);
		CHECK(Assets[0].first == "Materials/0");
		CHECK(Assets[0].second->IsA<WMaterial>());
		CHECK(Assets[1].first == "Meshes/0/Primitives/0");
		CHECK(Assets[1].second->IsA<WStaticMesh>());

		CHECK_FALSE(FGltfImporter::LoadAssets(Directory.GetPath() / "Missing.gltf", Assets, Error));
		CHECK_FALSE(Error.empty());
	}
}
