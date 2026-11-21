#pragma once

#include "CoreMinimal.h"

#include <cstddef>
#include <type_traits>

class WObject;

/**
 * The untyped reference held by every TObjectPtr.
 *
 * All strong object references have this one representation, which is what lets the garbage collector and other
 * reference visitors read and rewrite them without knowing the referenced type.
 */
class FObjectPtr
{
public:
	constexpr FObjectPtr() = default;

	explicit FObjectPtr(WObject* InObject)
		: Object(InObject)
	{
	}

	/** Returns the referenced object, or null. */
	WObject* Get() const
	{
		return Object;
	}

	/** Makes the reference null. */
	void Reset()
	{
		Object = nullptr;
	}

private:
	WObject* Object = nullptr;
};

/**
 * A strong reference to a WObject.
 *
 * A TObjectPtr that is a WPROPERTY keeps its target alive: the garbage collector follows it, and sets it to null
 * if the target is destroyed. It converts to and from a plain pointer, so it is used exactly like one. A TObjectPtr
 * that is not reachable through reflection (a local variable, an unreflected member) does not keep anything alive.
 *
 * ObjectType may be an incomplete type where a TObjectPtr is declared; it must be complete where it is used.
 */
template <typename ObjectType>
class TObjectPtr : public FObjectPtr
{
public:
	constexpr TObjectPtr() = default;

	constexpr TObjectPtr(std::nullptr_t)
	{
	}

	TObjectPtr(ObjectType* InObject)
		: FObjectPtr(InObject)
	{
	}

	/** Converts from a reference to a derived type. */
	template <typename OtherType>
		requires std::is_convertible_v<OtherType*, ObjectType*>
	TObjectPtr(const TObjectPtr<OtherType>& Other)
		: FObjectPtr(Other.FObjectPtr::Get())
	{
	}

	/** Returns the referenced object, or null. */
	ObjectType* Get() const
	{
		return static_cast<ObjectType*>(FObjectPtr::Get());
	}

	operator ObjectType*() const
	{
		return Get();
	}

	ObjectType* operator->() const
	{
		return Get();
	}

	ObjectType& operator*() const
	{
		return *Get();
	}
};
