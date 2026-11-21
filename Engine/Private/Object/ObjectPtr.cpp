#include "Object/ObjectPtr.h"

#include "Object/ObjectSystem.h"

WObject* FObjectPtr::ResolveNonNullForStore(WObject* InObject)
{
	return FObjectSystem::Get().HandleReferenceStored(InObject);
}
