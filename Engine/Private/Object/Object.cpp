#include "Object/Object.h"

#include "Object/Class.h"
#include "Object/ObjectSystem.h"

WObject::WObject()
{
	FObjectSystem::HandleObjectConstruction(this);
}

WObject::~WObject()
{
	FObjectSystem::HandleObjectDestruction(this);
}

WClass* WObject::StaticClass()
{
	return FObjectSystem::Get().GetObjectClass();
}

bool WObject::IsA(const WClass* InClass) const
{
	return InClass != nullptr && ClassPrivate->IsChildOf(InClass);
}

void WObject::AddToRoot()
{
	FObjectSystem::Get().AddToRoot(this);
}

void WObject::RemoveFromRoot()
{
	FObjectSystem::Get().RemoveFromRoot(this);
}

bool WObject::IsRooted() const
{
	return HasInternalFlag(EInternalFlags::Rooted);
}

void WObject::Destroy()
{
	FObjectSystem::Get().DestroyObject(this);
}

void WObject::BeginDestroy()
{
	WW_CHECKF(HasInternalFlag(EInternalFlags::BeginDestroyCalled),
		"BeginDestroy was called directly on a {}. Only the garbage collector calls it.",
		ClassPrivate->GetName());

	SetInternalFlag(EInternalFlags::BeginDestroyReachedBase);
}
