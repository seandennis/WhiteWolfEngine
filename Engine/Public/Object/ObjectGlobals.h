#pragma once

#include "CoreMinimal.h"

#include "Object/Class.h"
#include "Object/Object.h"

/**
 * Creates an object of the given class, which must not be null or abstract. The new object is owned by the garbage
 * collector: keep it in a reflected TObjectPtr property of a reachable object, or add it to the root set, or it is
 * destroyed by the next collection.
 */
ENGINE_API WObject* NewObject(WClass* Class);

/**
 * Creates an object of type ObjectType.
 *
 * @param Class The class to instantiate, when it should be a class derived from ObjectType.
 */
template <typename ObjectType>
ObjectType* NewObject(WClass* Class = ObjectType::StaticClass())
{
	WW_CHECK(Class != nullptr);
	WW_CHECKF(Class->IsChildOf(ObjectType::StaticClass()),
		"NewObject: class {} does not derive from {}",
		Class->GetName(),
		ObjectType::StaticClass()->GetName());
	return static_cast<ObjectType*>(NewObject(Class));
}

/** Returns Object as a TargetType if it is one, or null if Object is null or of another class. */
template <typename TargetType>
TargetType* Cast(WObject* Object)
{
	return Object != nullptr && Object->IsA(TargetType::StaticClass()) ? static_cast<TargetType*>(Object) : nullptr;
}

/** Returns Object as a TargetType if it is one, or null if Object is null or of another class. */
template <typename TargetType>
const TargetType* Cast(const WObject* Object)
{
	return Object != nullptr && Object->IsA(TargetType::StaticClass()) ? static_cast<const TargetType*>(Object)
																	   : nullptr;
}

/**
 * Destroys every object that is not reachable from the root set, before returning.
 *
 * Collection normally runs by itself; this function exists for tests and tools. Call it only from a point where no
 * code further up the call stack holds a plain pointer to an object that nothing else keeps alive.
 */
ENGINE_API void CollectGarbage();
