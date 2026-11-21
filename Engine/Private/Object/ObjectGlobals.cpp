#include "Object/ObjectGlobals.h"

#include "Object/ObjectSystem.h"

WObject* NewObject(WClass* Class)
{
	return FObjectSystem::Get().CreateObject(Class);
}

bool IsValid(const WObject* Object)
{
	return Object != nullptr && FObjectSystem::Get().IsObjectValid(Object);
}

void CollectGarbage()
{
	FObjectSystem::Get().CollectGarbage();
}
