#pragma once

#include "CoreMinimal.h"

#include "Object/Object.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Reflection/ArrayProperty.h"
#include "Reflection/ObjectProperty.h"
#include "Reflection/Property.h"
#include "Reflection/WeakObjectProperty.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * Creates the FProperty that describes a value of type ValueType.
 *
 * There is one specialization for each kind of type that can be a reflected member variable. Using a type that has
 * none is a compile error: such a type cannot be a WPROPERTY.
 */
template <typename ValueType>
struct TPropertyFactory;

/** TObjectPtr<ObjectType>: a strong reference to an object. */
template <typename ObjectType>
struct TPropertyFactory<TObjectPtr<ObjectType>>
{
	static std::unique_ptr<FProperty> Create(std::string Name, FProperty::FValueAccessor Accessor)
	{
		static_assert(std::is_base_of_v<WObject, ObjectType>, "TObjectPtr properties must refer to WObject classes");
		return std::make_unique<FObjectProperty>(std::move(Name), Accessor, &ObjectType::StaticClass);
	}
};

/** TWeakObjectPtr<ObjectType>: a reference to an object that does not keep it alive. */
template <typename ObjectType>
struct TPropertyFactory<TWeakObjectPtr<ObjectType>>
{
	static std::unique_ptr<FProperty> Create(std::string Name, FProperty::FValueAccessor Accessor)
	{
		static_assert(
			std::is_base_of_v<WObject, ObjectType>, "TWeakObjectPtr properties must refer to WObject classes");
		return std::make_unique<FWeakObjectProperty>(std::move(Name), Accessor, &ObjectType::StaticClass);
	}
};

/** std::vector<ElementType>: an array of any reflectable type. */
template <typename ElementType>
struct TPropertyFactory<std::vector<ElementType>>
{
	static std::unique_ptr<FProperty> Create(std::string Name, FProperty::FValueAccessor Accessor)
	{
		static_assert(!std::is_same_v<ElementType, bool>, "std::vector<bool> has no addressable elements");
		return std::make_unique<TArrayProperty<ElementType>>(
			std::move(Name), Accessor, TPropertyFactory<ElementType>::Create(std::string(), nullptr));
	}
};

/** Breaks a pointer to a member variable into the class that declares it and the type of the member. */
template <auto Member>
struct TMemberPointerTraits;

template <typename OwnerType, typename ValueType, ValueType OwnerType::*Member>
struct TMemberPointerTraits<Member>
{
	using FOwner = OwnerType;
	using FValue = ValueType;

	/** Returns the address of the member inside Container, which is a WObject* that points to an FOwner. */
	static void* GetValuePtr(void* Container)
	{
		static_assert(
			std::is_base_of_v<WObject, OwnerType>, "Reflected member variables must belong to a WObject class");
		return &(static_cast<OwnerType*>(static_cast<WObject*>(Container))->*Member);
	}
};

/**
 * Creates the property that describes a member variable:
 *
 *     Class.AddProperty(MakeProperty<&WDoor::Hinge>("Hinge"));
 */
template <auto Member>
std::unique_ptr<FProperty> MakeProperty(std::string Name)
{
	using FTraits = TMemberPointerTraits<Member>;
	return TPropertyFactory<typename FTraits::FValue>::Create(std::move(Name), &FTraits::GetValuePtr);
}
