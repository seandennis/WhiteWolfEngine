#include "Object/ObjectGlobals.h"

#include "Object/ObjectSystem.h"

WObject* NewObject(WClass* Class)
{
	return FObjectSystem::Get().CreateObject(Class);
}

void CollectGarbage()
{
	FObjectSystem::Get().CollectGarbage();
}
