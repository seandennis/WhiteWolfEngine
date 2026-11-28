#include "Assets/AssetReferenceContext.h"

#include "Assets/AssetManager.h"
#include "Assets/TestAssets.h"
#include "Json/JsonValue.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Rendering/Material.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("FAssetReferenceContext")
{
	TEST_CASE("An asset with a path is written as its path")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		std::string Error;
		const WTexture* const Wood = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);
		REQUIRE(Wood != nullptr);
		const WTexture* const WoodData = FAssetManager::Load<WTexture>("Textures/Wood.png#Linear", Error);
		REQUIRE(WoodData != nullptr);
		FAssetReferenceContext Context;

		CHECK(Context.ExportObjectReference(*Wood) == FJsonValue("Textures/Wood.png"));
		CHECK(Context.ExportObjectReference(*WoodData) == FJsonValue("Textures/Wood.png#Linear"));
		CHECK(Context.GetUnwritten().empty());
	}

	TEST_CASE("What has no asset path is written as null and named, and nothing is logged")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		const WMaterial* const MadeInCode = NewObject<WMaterial>();
		WScene* const Scene = NewObject<WScene>();
		WEntity* const Entity = Scene->CreateEntity("Lamp");
		REQUIRE(Entity != nullptr);
		const WPointLightComponent* const Light = Entity->AddComponent<WPointLightComponent>();
		REQUIRE(Light != nullptr);
		FAssetReferenceContext Context;

		CHECK(Context.ExportObjectReference(*MadeInCode).IsNull());
		CHECK(Context.ExportObjectReference(*Entity).IsNull());
		CHECK(Context.ExportObjectReference(*Light).IsNull());

		const std::vector<std::string> Expected = {"WMaterial", "WEntity", "WPointLightComponent"};
		CHECK(Context.GetUnwritten() == Expected);
		CHECK(Capture.GetMessages().empty());
		Scene->Destroy();
	}

	TEST_CASE("A path is found by loading it, as the same object that the asset manager loads")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		FAssetReferenceContext Context;
		WObject* Found = nullptr;
		std::string Error;

		REQUIRE(Context.ImportObjectReference(FJsonValue("Textures/Wood.png"), Found, Error));

		const WTexture* const Wood = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);
		REQUIRE(Wood != nullptr);
		CHECK(Found == Wood);
	}

	TEST_CASE("Without a content directory no path can be found, and the reason says so")
	{
		const FScopedObjectTest ObjectTest;
		REQUIRE(FAssetManager::GetContentDirectory().empty());
		FAssetReferenceContext Context;
		WObject* Found = nullptr;
		std::string Error;

		CHECK_FALSE(Context.ImportObjectReference(FJsonValue("Textures/Wood.png"), Found, Error));

		CHECK(Error == "the asset file \"Textures/Wood.png\" cannot be loaded: there is no content directory");
		CHECK(Found == nullptr);
	}

	TEST_CASE("Why a path cannot be loaded is the asset manager's reason, and the object is left alone")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		std::string Path;
		std::string Expected;
		FAssetReferenceContext Context;

		SUBCASE("Text that is no asset path")
		{
			Path = "../Wood.png";
			Expected = "\"../Wood.png\" is not the path of an asset: it must not have the part \"..\"";
		}
		SUBCASE("A file that is not there")
		{
			Path = "Textures/Missing.png";
			Expected = "the content directory has no file \"Textures/Missing.png\"";
		}
		SUBCASE("A key that the file does not have")
		{
			Path = "Models/Square.gltf#Meshes/7/Primitives/0";
			Expected = "the file \"Models/Square.gltf\" has no asset \"Meshes/7/Primitives/0\"";
		}

		WObject* const Before = NewObject<WMaterial>();
		WObject* Found = Before;
		std::string Error;
		CHECK_FALSE(Context.ImportObjectReference(FJsonValue(Path), Found, Error));
		CHECK(Error == Expected);
		CHECK(Found == Before);
	}

	TEST_CASE("Only a string is the path of an asset: a number names an object of one scene, and is refused")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		FAssetReferenceContext Context;
		WObject* const Before = NewObject<WMaterial>();

		for (const FJsonValue& Json :
			{FJsonValue(3), FJsonValue(true), FJsonValue::MakeArray(), FJsonValue::MakeObject(), FJsonValue(1.5)})
		{
			const std::string Kind(Json.GetTypeDescription());
			CAPTURE(Kind);
			WObject* Found = Before;
			std::string Error;
			CHECK_FALSE(Context.ImportObjectReference(Json, Found, Error));
			CHECK(Error == "expected the path of an asset, but found " + Kind);
			CHECK(Found == Before);
		}
	}
}
