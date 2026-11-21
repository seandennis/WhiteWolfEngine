#include "Scene/Entity.h"

#include "Object/Class.h"
#include "Scene/Scene.h"

#include <utility>

const std::string& WEntity::GetName() const
{
	return Name;
}

void WEntity::SetName(const std::string& NewName)
{
	Name = NewName;
}

int64 WEntity::GetId() const
{
	return Id;
}

WScene* WEntity::GetScene() const
{
	return Scene;
}

WEntity* WEntity::GetParent() const
{
	return Parent;
}

const std::vector<TObjectPtr<WEntity>>& WEntity::GetChildren() const
{
	return Children;
}

bool WEntity::SetParent(WEntity* NewParent, ETransformRule TransformRule)
{
	if (!IsValid(this))
	{
		WW_LOG(LogScene, Warning, "SetParent: entity '{}' has been destroyed", Name);
		return false;
	}
	if (NewParent == Parent.Get())
	{
		return true;
	}
	if (NewParent != nullptr)
	{
		if (!IsValid(NewParent))
		{
			WW_LOG(LogScene, Warning, "SetParent: the new parent of entity '{}' has been destroyed", Name);
			return false;
		}
		if (NewParent->Scene.Get() != Scene.Get())
		{
			WW_LOG(LogScene,
				Warning,
				"SetParent: entity '{}' and its new parent '{}' are in different scenes",
				Name,
				NewParent->Name);
			return false;
		}
		if (NewParent == this || NewParent->IsDescendantOf(this))
		{
			WW_LOG(LogScene,
				Warning,
				"SetParent: entity '{}' cannot become a child of '{}', which is the entity itself or one of its "
				"descendants",
				Name,
				NewParent->Name);
			return false;
		}
	}

	const FTransform WorldTransform = GetWorldTransform();

	std::vector<TObjectPtr<WEntity>>* const OldSiblings = GetSiblings();
	if (OldSiblings != nullptr)
	{
		std::erase_if(*OldSiblings, [this](const TObjectPtr<WEntity>& Sibling) { return Sibling.Get() == this; });
	}

	Parent = NewParent;

	std::vector<TObjectPtr<WEntity>>* const NewSiblings = GetSiblings();
	if (NewSiblings != nullptr)
	{
		NewSiblings->push_back(this);
	}

	if (TransformRule == ETransformRule::KeepWorld)
	{
		SetWorldTransform(WorldTransform);
	}

	return true;
}

bool WEntity::IsDescendantOf(const WEntity* Other) const
{
	for (const WEntity* Ancestor = Parent; Ancestor != nullptr; Ancestor = Ancestor->Parent)
	{
		if (Ancestor == Other)
		{
			return true;
		}
	}

	return false;
}

const FTransform& WEntity::GetRelativeTransform() const
{
	return RelativeTransform;
}

void WEntity::SetRelativeTransform(const FTransform& NewTransform)
{
	RelativeTransform = NewTransform;
}

FTransform WEntity::GetWorldTransform() const
{
	const WEntity* const ParentEntity = Parent;
	return ParentEntity != nullptr ? FTransform::Combine(ParentEntity->GetWorldTransform(), RelativeTransform)
								   : RelativeTransform;
}

void WEntity::SetWorldTransform(const FTransform& NewTransform)
{
	const WEntity* const ParentEntity = Parent;
	RelativeTransform =
		ParentEntity != nullptr ? NewTransform.GetRelativeTransform(ParentEntity->GetWorldTransform()) : NewTransform;
}

FVector WEntity::GetWorldLocation() const
{
	return GetWorldTransform().Location;
}

void WEntity::SetWorldLocation(const FVector& NewLocation)
{
	FTransform WorldTransform = GetWorldTransform();
	WorldTransform.Location = NewLocation;
	SetWorldTransform(WorldTransform);
}

WComponent* WEntity::AddComponent(WClass* ComponentClass)
{
	if (!IsValid(this))
	{
		WW_LOG(LogScene, Warning, "AddComponent: entity '{}' has been destroyed", Name);
		return nullptr;
	}
	if (ComponentClass == nullptr)
	{
		WW_LOG(LogScene, Warning, "AddComponent: no component class was given for entity '{}'", Name);
		return nullptr;
	}
	if (!ComponentClass->IsChildOf(WComponent::StaticClass()))
	{
		WW_LOG(LogScene, Warning, "AddComponent: {} is not a component class", ComponentClass->GetName());
		return nullptr;
	}
	if (ComponentClass->IsAbstract())
	{
		WW_LOG(LogScene, Warning, "AddComponent: {} is abstract and cannot be created", ComponentClass->GetName());
		return nullptr;
	}

	WScene* const OwningScene = Scene;
	return AddComponentWithId(ComponentClass, OwningScene != nullptr ? OwningScene->AllocateId() : 0);
}

WComponent* WEntity::AddComponentWithId(WClass* ComponentClass, int64 ComponentId)
{
	WComponent* const Component = NewObject<WComponent>(ComponentClass);
	Component->Owner = this;
	Component->Id = ComponentId;
	Components.push_back(Component);

	WScene* const OwningScene = Scene;
	if (OwningScene != nullptr)
	{
		OwningScene->RegisterObject(ComponentId, *Component);
	}

	Component->OnAttached();
	return Component;
}

const std::vector<TObjectPtr<WComponent>>& WEntity::GetComponents() const
{
	return Components;
}

WComponent* WEntity::FindComponent(const WClass* ComponentClass) const
{
	for (const TObjectPtr<WComponent>& Component : Components)
	{
		if (Component != nullptr && Component->IsA(ComponentClass))
		{
			return Component;
		}
	}

	return nullptr;
}

bool WEntity::RemoveComponent(WComponent* Component)
{
	if (Component == nullptr || Component->Owner.Get() != this)
	{
		return false;
	}

	Component->Destroy();
	return true;
}

void WEntity::DetachComponent(WComponent& Component)
{
	Component.OnDetached();

	WScene* const OwningScene = Scene;
	if (OwningScene != nullptr)
	{
		OwningScene->UnregisterObject(Component.Id);
	}
	Component.Owner = nullptr;
}

void WEntity::HandleComponentDestroyed(WComponent& Component)
{
	DetachComponent(Component);
	std::erase_if(
		Components, [&Component](const TObjectPtr<WComponent>& Existing) { return Existing.Get() == &Component; });
}

std::vector<TObjectPtr<WEntity>>* WEntity::GetSiblings() const
{
	WEntity* const ParentEntity = Parent;
	if (ParentEntity != nullptr)
	{
		return &ParentEntity->Children;
	}

	WScene* const OwningScene = Scene;
	return OwningScene != nullptr ? &OwningScene->RootEntities : nullptr;
}

void WEntity::OnDestroyed()
{
	Super::OnDestroyed();

	// Leave the hierarchy first, so that nothing reachable lists a destroyed entity.
	std::vector<TObjectPtr<WEntity>>* const Siblings = GetSiblings();
	if (Siblings != nullptr)
	{
		std::erase_if(*Siblings, [this](const TObjectPtr<WEntity>& Sibling) { return Sibling.Get() == this; });
	}

	// The lists are emptied before their contents are destroyed, because each component and child that is
	// destroyed looks for itself in the list of its owner.
	const std::vector<TObjectPtr<WComponent>> RemovedComponents = std::exchange(Components, {});
	for (const TObjectPtr<WComponent>& Component : RemovedComponents)
	{
		if (Component != nullptr)
		{
			DetachComponent(*Component);
			Component->Destroy();
		}
	}

	const std::vector<TObjectPtr<WEntity>> RemovedChildren = std::exchange(Children, {});
	for (const TObjectPtr<WEntity>& Child : RemovedChildren)
	{
		if (Child != nullptr)
		{
			Child->Destroy();
		}
	}

	WScene* const OwningScene = Scene;
	if (OwningScene != nullptr)
	{
		OwningScene->UnregisterObject(Id);
	}
	Parent = nullptr;
	Scene = nullptr;
}
