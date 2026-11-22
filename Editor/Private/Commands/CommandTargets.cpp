#include "Commands/CommandTargets.h"

#include "Editor.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <format>

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
