#pragma once

#include "CoreMinimal.h"

#include <string>

class IObjectReferenceVisitor;

/**
 * Describes one reflected member variable of a class: its name, where its value lives inside an object, and how to
 * work with a value of its type. Each kind of type has its own FProperty subclass.
 *
 * Two kinds of pointer appear in this interface. A container pointer is the object that owns the member: a WObject*
 * for a class member. A value pointer is the address of the member's value itself, as returned by GetValuePtr.
 */
class ENGINE_API FProperty
{
public:
	/** Returns the address of the property's value inside the container that owns it. */
	using FValueAccessor = void* (*)(void* Container);

	FProperty(const FProperty&) = delete;
	FProperty& operator=(const FProperty&) = delete;
	virtual ~FProperty();

	/** Returns the name of the member variable. */
	const std::string& GetName() const
	{
		return Name;
	}

	/**
	 * Returns the address of this property's value inside Container, which must be an instance of the class that
	 * declares the property (or of a class derived from it), passed as a WObject*.
	 *
	 * Only properties that describe a member variable have a location; calling this on the element property of an
	 * array is a fatal error.
	 */
	void* GetValuePtr(void* Container) const;

	/** Returns true if a value of this property can hold strong references to objects. */
	virtual bool ContainsObjectReferences() const;

	/** Passes every strong object reference inside the value at Value to Visitor. */
	virtual void VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const;

protected:
	/**
	 * @param InName The member's name; empty for the element property of a container.
	 * @param InAccessor Locates the value inside its container; null for the element property of a container.
	 */
	FProperty(std::string InName, FValueAccessor InAccessor);

private:
	std::string Name;
	FValueAccessor Accessor;
};
