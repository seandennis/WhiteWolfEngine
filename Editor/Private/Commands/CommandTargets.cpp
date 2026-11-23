#include "Commands/CommandTargets.h"

#include "Editor.h"
#include "Json/JsonValue.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <format>
#include <utility>

WEntity* FCommandTargets::FindEntity(const FEditor& Editor, int64 EntityId, std::string& OutError)
{
	WEntity* const Entity = Editor.GetScene().FindEntityById(EntityId);
	if (Entity == nullptr)
	{
		OutError = std::format("There is no entity with the identifier {}.", EntityId);
	}
	return Entity;
}

WComponent* FCommandTargets::FindComponent(const FEditor& Editor, int64 ComponentId, std::string& OutError)
{
	WComponent* const Component = Editor.GetScene().FindComponentById(ComponentId);
	if (Component == nullptr)
	{
		OutError = std::format("There is no component with the identifier {}.", ComponentId);
	}
	return Component;
}

WObject* FCommandTargets::FindObject(const FEditor& Editor, int64 ObjectId, std::string& OutError)
{
	const WScene& Scene = Editor.GetScene();
	if (WEntity* const Entity = Scene.FindEntityById(ObjectId))
	{
		return Entity;
	}
	if (WComponent* const Component = Scene.FindComponentById(ObjectId))
	{
		return Component;
	}
	OutError = std::format("There is no entity or component with the identifier {}.", ObjectId);
	return nullptr;
}

bool FCommandTargets::ReadEntityIds(
	const FEditor& Editor, const FJsonValue& Entities, std::vector<int64>& OutEntityIds, std::string& OutError)
{
	if (!Entities.IsArray())
	{
		OutError =
			std::format("Entities must be an array of entity identifiers, but is {}.", Entities.GetTypeDescription());
		return false;
	}

	std::vector<int64> EntityIds;
	for (const FJsonValue& Element : Entities.GetElements())
	{
		if (!Element.IsInteger())
		{
			OutError = std::format("Entities must hold entity identifiers, which are integers, but holds {}.",
				Element.GetTypeDescription());
			return false;
		}
		if (FindEntity(Editor, Element.GetInteger(), OutError) == nullptr)
		{
			return false;
		}
		EntityIds.push_back(Element.GetInteger());
	}

	OutEntityIds = std::move(EntityIds);
	return true;
}
