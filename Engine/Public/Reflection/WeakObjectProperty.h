#pragma once

#include "CoreMinimal.h"

#include "Reflection/Property.h"

class WClass;
class WObject;

/**
 * A property whose value is a TWeakObjectPtr: a reference to an object of a given class that does not keep the
 * object alive. The garbage collector does not follow it. It is saved and loaded like a strong reference.
 */
class ENGINE_API FWeakObjectProperty final : public FProperty
{
public:
	/** Returns the class that the referenced objects must be instances of. */
	using FClassGetter = WClass* (*)();

	/**
	 * @param InGetPropertyClass Returns the class of object this property refers to. It is not called until the
	 * class is first needed.
	 */
	FWeakObjectProperty(std::string InName, FValueAccessor InAccessor, FClassGetter InGetPropertyClass);

	/** Returns the class that the referenced objects must be instances of. */
	WClass* GetPropertyClass() const;

	/**
	 * Returns the object that a reference refers to, or null if it refers to none or the object has been
	 * destroyed.
	 *
	 * @param Value The address of the reference, a value of this property (GetValuePtr).
	 */
	WObject* GetReferencedObject(const void* Value) const;

	std::string GetTypeName() const override;
	FJsonValue ExportJson(const void* Value, IPropertyJsonContext& Context) const override;
	bool ImportJson(
		void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const override;

private:
	FClassGetter GetPropertyClassFunction;
};
