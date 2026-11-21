#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectPtr.h"

#include <cstddef>
#include <type_traits>

class WObject;

/**
 * The untyped reference held by every TWeakObjectPtr: the slot of an object in the object table, together with the
 * slot's serial number at the time, which tells the object apart from any later occupant of the same slot.
 */
class FWeakObjectPtr
{
public:
	constexpr FWeakObjectPtr() = default;

	/** Refers to Object, which may be null. */
	ENGINE_API explicit FWeakObjectPtr(const WObject* Object);

	/**
	 * Returns the object, or null if this reference is null, or the object has been destroyed, has been marked for
	 * destruction with Destroy, or is being destroyed by a collection that is in progress.
	 */
	ENGINE_API WObject* Get() const;

	/** Returns true if Get would return an object. */
	bool IsValid() const
	{
		return Get() != nullptr;
	}

	/** Makes the reference null. */
	void Reset()
	{
		*this = FWeakObjectPtr();
	}

	/**
	 * Two weak references are equal if they refer to the same object, whether or not it still exists, or if both
	 * were null from the start.
	 */
	bool operator==(const FWeakObjectPtr& Other) const = default;

private:
	int32 ObjectIndex = -1;
	uint32 SerialNumber = 0;
};

/**
 * A reference to a WObject that does not keep it alive.
 *
 * Get returns null once the target has been destroyed or marked for destruction, so a weak reference never dangles.
 * Use it for references that must not extend the target's lifetime: back-pointers, caches, observers.
 */
template <typename ObjectType>
class TWeakObjectPtr : public FWeakObjectPtr
{
public:
	constexpr TWeakObjectPtr() = default;

	constexpr TWeakObjectPtr(std::nullptr_t)
	{
	}

	TWeakObjectPtr(ObjectType* Object)
		: FWeakObjectPtr(Object)
	{
	}

	template <typename OtherType>
		requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakObjectPtr(const TObjectPtr<OtherType>& Object)
		: FWeakObjectPtr(Object.Get())
	{
	}

	/** Converts from a weak reference to a derived type. */
	template <typename OtherType>
		requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakObjectPtr(const TWeakObjectPtr<OtherType>& Other)
		: FWeakObjectPtr(Other)
	{
	}

	/** Returns the object, or null if it no longer exists or is marked for destruction. */
	ObjectType* Get() const
	{
		return static_cast<ObjectType*>(FWeakObjectPtr::Get());
	}
};
