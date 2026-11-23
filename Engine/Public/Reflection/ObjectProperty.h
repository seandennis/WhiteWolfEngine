#pragma once

#include "CoreMinimal.h"

#include "Reflection/Property.h"

class WClass;
class WObject;

/**
 * A property whose value is a TObjectPtr: a strong reference to an object of a given class.
 *
 * In JSON the reference is null or whatever the IPropertyJsonContext writes to identify the object. Loading a
 * reference to an object that is not of the property's class is an error.
 */
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

	/**
	 * Returns the object that a reference refers to, or null if it refers to none.
	 *
	 * @param Value The address of the reference, a value of this property (GetValuePtr).
	 */
	WObject* GetReferencedObject(const void* Value) const;

	std::string GetTypeName() const override;
	bool ContainsObjectReferences() const override;
	void VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const override;
	FJsonValue ExportJson(const void* Value, IPropertyJsonContext& Context) const override;
	bool ImportJson(
		void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const override;

private:
	FClassGetter GetPropertyClassFunction;
};
