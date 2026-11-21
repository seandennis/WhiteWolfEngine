#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/PointLightComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Scene/TestSceneObjects.h"

#include <doctest/doctest.h>

#include <numbers>
#include <string>

TEST_SUITE("WDirectionalLightComponent")
{
	TEST_CASE("A new directional light is white, visible and lights a white matte surface to about one")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		const WDirectionalLightComponent* const Light =
			Scene->CreateEntity("Sun")->AddComponent<WDirectionalLightComponent>();

		REQUIRE(Light != nullptr);
		CHECK(Light->Color == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		CHECK(Light->Intensity == std::numbers::pi_v<float>);
		CHECK(Light->bVisible);
	}

	TEST_CASE("A directional light is saved and loaded with its scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WDirectionalLightComponent* const Light =
			Scene->CreateEntity("Sun")->AddComponent<WDirectionalLightComponent>();
		Light->Color = FLinearColor(1.0f, 0.5f, 0.25f, 1.0f);
		Light->Intensity = 100000.0f;
		Light->bVisible = false;

		std::string Error;
		WScene* const Loaded = FSceneSerializer::LoadFromText(FSceneSerializer::SaveToText(*Scene.Get()), Error);

		INFO("Error: ", Error);
		REQUIRE(Loaded != nullptr);
		REQUIRE(Loaded->GetRootEntities().size() == 1);
		const WDirectionalLightComponent* const LoadedLight =
			Loaded->GetRootEntities().front()->FindComponent<WDirectionalLightComponent>();
		REQUIRE(LoadedLight != nullptr);
		CHECK(LoadedLight->Color == FLinearColor(1.0f, 0.5f, 0.25f, 1.0f));
		CHECK(LoadedLight->Intensity == 100000.0f);
		CHECK_FALSE(LoadedLight->bVisible);
	}
}

TEST_SUITE("WPointLightComponent")
{
	TEST_CASE("A new point light is white, visible and has no range limit")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		const WPointLightComponent* const Light = Scene->CreateEntity("Lamp")->AddComponent<WPointLightComponent>();

		REQUIRE(Light != nullptr);
		CHECK(Light->Color == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		CHECK(Light->Intensity == std::numbers::pi_v<float>);
		CHECK(Light->Range == 0.0f);
		CHECK(Light->bVisible);
	}

	TEST_CASE("A point light is saved and loaded with its scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WPointLightComponent* const Light = Scene->CreateEntity("Lamp")->AddComponent<WPointLightComponent>();
		Light->Color = FLinearColor(0.25f, 0.5f, 1.0f, 1.0f);
		Light->Intensity = 800.0f;
		Light->Range = 12.5f;
		Light->bVisible = false;

		std::string Error;
		WScene* const Loaded = FSceneSerializer::LoadFromText(FSceneSerializer::SaveToText(*Scene.Get()), Error);

		INFO("Error: ", Error);
		REQUIRE(Loaded != nullptr);
		REQUIRE(Loaded->GetRootEntities().size() == 1);
		const WPointLightComponent* const LoadedLight =
			Loaded->GetRootEntities().front()->FindComponent<WPointLightComponent>();
		REQUIRE(LoadedLight != nullptr);
		CHECK(LoadedLight->Color == FLinearColor(0.25f, 0.5f, 1.0f, 1.0f));
		CHECK(LoadedLight->Intensity == 800.0f);
		CHECK(LoadedLight->Range == 12.5f);
		CHECK_FALSE(LoadedLight->bVisible);
	}
}
