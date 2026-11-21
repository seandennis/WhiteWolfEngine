#include "Scene/Component.h"

#include "Scene/Entity.h"
#include "Scene/Scene.h"

WEntity* WComponent::GetOwner() const
{
	return Owner;
}

WScene* WComponent::GetScene() const
{
	const WEntity* const OwnerEntity = Owner;
	return OwnerEntity != nullptr ? OwnerEntity->GetScene() : nullptr;
}

int64 WComponent::GetId() const
{
	return Id;
}

void WComponent::OnAttached()
{
}

void WComponent::OnDetached()
{
}

void WComponent::OnDestroyed()
{
	Super::OnDestroyed();

	// A component that is destroyed on its own leaves its entity. When the entity is being destroyed it has
	// detached its components already, and there is no owner left here.
	WEntity* const OwnerEntity = Owner;
	if (OwnerEntity != nullptr)
	{
		OwnerEntity->HandleComponentDestroyed(*this);
	}
}
