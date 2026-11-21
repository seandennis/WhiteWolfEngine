#include "Object/WeakObjectPtr.h"

#include "Object/ObjectSystem.h"

FWeakObjectPtr::FWeakObjectPtr(const WObject* Object)
{
	if (Object != nullptr)
	{
		FObjectSystem::Get().GetWeakReference(Object, ObjectIndex, SerialNumber);
	}
}

WObject* FWeakObjectPtr::Get() const
{
	// A null reference is answered without touching the object system, so that weak references can be created and
	// tested before the object system exists and after it is gone.
	return ObjectIndex >= 0 ? FObjectSystem::Get().ResolveWeakReference(ObjectIndex, SerialNumber) : nullptr;
}
