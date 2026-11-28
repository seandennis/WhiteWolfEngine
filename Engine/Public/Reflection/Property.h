#pragma once

#include "CoreMinimal.h"

#include "Core/EnumClassFlags.h"
#include "Reflection/PropertyFlags.h"
#include "Reflection/PropertySpecifiers.h"

#include <string>

class FJsonValue;
class IObjectReferenceVisitor;
class IPropertyJsonContext;
class WObject;

/**
 * Describes one reflected value: a member variable of a class, an element of an array, or a parameter of a reflected
 * function. It knows the value's name and type, where the value lives inside an object, and how to convert a value
 * to and from JSON. Each kind of type has its own FProperty subclass.
 *
 * Two kinds of pointer appear in this interface. A container is the object that owns a member variable. A value
 * pointer is the address of the value itself, as returned by GetValuePtr.
 */
class ENGINE_API FProperty
{
public:
	/** Returns the address of the property's value inside the container that owns it. */
	using FValueAccessor = void* (*)(void* Container);

	FProperty(const FProperty&) = delete;
	FProperty& operator=(const FProperty&) = delete;
	virtual ~FProperty();

	/** Returns the name of the member variable or parameter. Empty for the element property of an array. */
	const std::string& GetName() const
	{
		return Name;
	}

	/** Returns the C++ type of the value as it is written in source: "int32", "FVector", "TObjectPtr<WDoor>". */
	virtual std::string GetTypeName() const = 0;

	EPropertyFlags GetFlags() const
	{
		return Specifiers.Flags;
	}

	/** Returns true if the property has at least one of the given flags. */
	bool HasAnyFlags(EPropertyFlags Flags) const
	{
		return EnumHasAnyFlags(Specifiers.Flags, Flags);
	}

	/** Returns the group the property is shown under in the editor, or an empty string for the default group. */
	const std::string& GetCategory() const
	{
		return Specifiers.Category;
	}

	/** Sets the property's flags and category. Called while the property is being created, before it is used. */
	void SetSpecifiers(FPropertySpecifiers InSpecifiers);

	/**
	 * Returns the address of this property's value inside Container, which must be an instance of the class that
	 * declares the property or of a class derived from it. Giving it an object of another class is undefined
	 * behaviour; look the property up in the object's own class to be safe.
	 *
	 * Only properties that describe a member variable have a location; calling this on the element property of an
	 * array or on a function parameter is a fatal error.
	 */
	void* GetValuePtr(WObject* Container) const;
	const void* GetValuePtr(const WObject* Container) const;

	/** Returns true if a value of this property can hold strong references to objects. */
	virtual bool ContainsObjectReferences() const;

	/** Passes every strong object reference inside the value at Value to Visitor. */
	virtual void VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const;

	/**
	 * Returns the value at Value as JSON.
	 *
	 * @param Context Writes references to other objects.
	 */
	virtual FJsonValue ExportJson(const void* Value, IPropertyJsonContext& Context) const = 0;

	/**
	 * Sets the value at Value from JSON.
	 *
	 * @param Context Finds the objects that references refer to.
	 * @param OutError Receives the reason on failure, for example "expected a number but found a string".
	 * @return True if the value was set. On failure the value is unchanged, an array's included.
	 */
	virtual bool ImportJson(
		void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const = 0;

protected:
	/**
	 * @param InName The name; empty for the element property of a container.
	 * @param InAccessor Locates the value inside its container; null for a value that is not a member variable.
	 */
	FProperty(std::string InName, FValueAccessor InAccessor);

private:
	std::string Name;
	FValueAccessor Accessor;
	FPropertySpecifiers Specifiers;
};
