#include "Scene/Scene.h"

#include "Object/GarbageCollector.h"

#include <algorithm>
#include <utility>

constinit FLogCategory LogScene("LogScene", ELogVerbosity::Log);

namespace
{
	/** Appends Entity and then its descendants, parents before children. */
	void AppendEntityTree(WEntity* Entity, std::vector<WEntity*>& OutEntities)
	{
		// An explicit stack, so that a very deep hierarchy cannot overflow the call stack.
		std::vector<WEntity*> Pending{Entity};
		while (!Pending.empty())
		{
			WEntity* const Current = Pending.back();
			Pending.pop_back();
			OutEntities.push_back(Current);

			// Pushed in reverse, so that the first child is the next entity taken.
			const std::vector<TObjectPtr<WEntity>>& Children = Current->GetChildren();
			for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator)
			{
				if (*Iterator != nullptr)
				{
					Pending.push_back(*Iterator);
				}
			}
		}
	}
}

const std::string& WScene::GetName() const
{
	return Name;
}

void WScene::SetName(const std::string& NewName)
{
	Name = NewName;
}

WEntity* WScene::CreateEntity(const std::string& EntityName, WEntity* ParentEntity)
{
	if (!IsValid(this))
	{
		WW_LOG(LogScene, Warning, "CreateEntity: scene '{}' has been destroyed", Name);
		return nullptr;
	}
	if (ParentEntity != nullptr)
	{
		if (!IsValid(ParentEntity))
		{
			WW_LOG(LogScene, Warning, "CreateEntity: the parent given for entity '{}' has been destroyed", EntityName);
			return nullptr;
		}
		if (ParentEntity->GetScene() != this)
		{
			WW_LOG(LogScene,
				Warning,
				"CreateEntity: the parent given for entity '{}' is not in scene '{}'",
				EntityName,
				Name);
			return nullptr;
		}
	}

	return CreateEntityWithId(EntityName, ParentEntity, AllocateId());
}

WEntity* WScene::CreateEntityWithId(const std::string& EntityName, WEntity* ParentEntity, int64 EntityId)
{
	WEntity* const Entity = NewObject<WEntity>();
	Entity->Name = EntityName;
	Entity->Id = EntityId;
	Entity->Scene = this;
	Entity->Parent = ParentEntity;
	RegisterObject(EntityId, *Entity);

	if (ParentEntity != nullptr)
	{
		ParentEntity->Children.push_back(Entity);
	}
	else
	{
		RootEntities.push_back(Entity);
	}

	return Entity;
}

const std::vector<TObjectPtr<WEntity>>& WScene::GetRootEntities() const
{
	return RootEntities;
}

std::vector<WEntity*> WScene::GetAllEntities() const
{
	std::vector<WEntity*> Entities;
	for (const TObjectPtr<WEntity>& RootEntity : RootEntities)
	{
		if (RootEntity != nullptr)
		{
			AppendEntityTree(RootEntity, Entities);
		}
	}
	return Entities;
}

WEntity* WScene::FindEntityById(int64 EntityId) const
{
	return Cast<WEntity>(FindObjectById(EntityId));
}

WComponent* WScene::FindComponentById(int64 ComponentId) const
{
	return Cast<WComponent>(FindObjectById(ComponentId));
}

WEntity* WScene::FindEntityByName(std::string_view EntityName) const
{
	const std::vector<WEntity*> Entities = GetAllEntities();
	const auto Found =
		std::ranges::find_if(Entities, [EntityName](const WEntity* Entity) { return Entity->GetName() == EntityName; });
	return Found != Entities.end() ? *Found : nullptr;
}

void WScene::DestroyAllEntities()
{
	// Each entity removes itself from RootEntities as it is destroyed, so the list is taken first.
	const std::vector<TObjectPtr<WEntity>> RemovedEntities = std::exchange(RootEntities, {});
	for (const TObjectPtr<WEntity>& Entity : RemovedEntities)
	{
		if (Entity != nullptr)
		{
			Entity->Destroy();
		}
	}
}

void WScene::OnDestroyed()
{
	Super::OnDestroyed();
	DestroyAllEntities();

	// A whole scene has just become garbage. Collecting it soon, rather than when the timer next comes round,
	// frees its memory and resources before the next scene needs them.
	FGarbageCollector::RequestCollection();
}

int64 WScene::AllocateId()
{
	return NextId++;
}

void WScene::RegisterObject(int64 ObjectId, const WObject& Object)
{
	ObjectsById[ObjectId] = FWeakObjectPtr(&Object);
	NextId = std::max(NextId, ObjectId + 1);
}

void WScene::UnregisterObject(int64 ObjectId)
{
	ObjectsById.erase(ObjectId);
}

WObject* WScene::FindObjectById(int64 ObjectId) const
{
	const auto Found = ObjectsById.find(ObjectId);
	return Found != ObjectsById.end() ? Found->second.Get() : nullptr;
}
