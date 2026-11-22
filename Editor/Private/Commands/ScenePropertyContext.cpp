#include "Commands/ScenePropertyContext.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Json/JsonValue.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <format>

FScenePropertyContext::FScenePropertyContext(const WScene& InScene)
	: Scene(InScene)
{
}

FJsonValue FScenePropertyContext::ExportObjectReference(const WObject& Object)
{
	const WEntity* const Entity = Cast<WEntity>(&Object);
	if (Entity != nullptr && Entity->GetScene() == &Scene)
	{
		return FJsonValue(Entity->GetId());
	}

	const WComponent* const Component = Cast<WComponent>(&Object);
	if (Component != nullptr && Component->GetScene() == &Scene)
	{
		return FJsonValue(Component->GetId());
	}

	const WAsset* const Asset = Cast<WAsset>(&Object);
	if (Asset != nullptr && !Asset->GetAssetPath().empty())
	{
		return FJsonValue(Asset->GetAssetPath());
	}

	return FJsonValue();
}

bool FScenePropertyContext::ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError)
{
	if (Json.IsString())
	{
		WAsset* const Asset = FAssetManager::Load(Json.GetString(), OutError);
		if (Asset == nullptr)
		{
			return false;
		}
		OutObject = Asset;
		return true;
	}
	if (!Json.IsInteger())
	{
		OutError =
			std::format("expected the identifier of an entity or component, or the path of an asset, but found {}",
				Json.GetTypeDescription());
		return false;
	}

	const int64 ObjectId = Json.GetInteger();
	if (WEntity* const Entity = Scene.FindEntityById(ObjectId))
	{
		OutObject = Entity;
		return true;
	}
	if (WComponent* const Component = Scene.FindComponentById(ObjectId))
	{
		OutObject = Component;
		return true;
	}

	OutError = std::format("there is no entity or component with the identifier {}", ObjectId);
	return false;
}
