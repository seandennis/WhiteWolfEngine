#pragma once

#include "CoreMinimal.h"

#include "Reflection/Property.h"

class WClass;

/** A property whose value is a TObjectPtr: a strong reference to an object of a given class. */
class ENGINE_API FObjectProperty final : public FProperty
{
public:
	/** Returns the class that the referenced objects must be instances of. */
	using FClassGetter = WClass* (*)();

	/**
	 * @param InGetPropertyClass Returns the class of object this property refers to. It is not called until the
	 * class is first needed, so a class may have properties that refer to itself or to classes registered later.
	 */
	FObjectProperty(std::string InName, FValueAccessor InAccessor, FClassGetter InGetPropertyClass);

	/** Returns the class that the referenced objects must be instances of. */
	WClass* GetPropertyClass() const;

	bool ContainsObjectReferences() const override;
	void VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const override;

private:
	FClassGetter GetPropertyClassFunction;
};
