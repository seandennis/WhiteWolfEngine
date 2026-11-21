#pragma once

#include "CoreMinimal.h"

#include "Reflection/Property.h"

class WClass;

/**
 * A property whose value is a TWeakObjectPtr: a reference to an object of a given class that does not keep the
 * object alive. The garbage collector does not follow it.
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

private:
	FClassGetter GetPropertyClassFunction;
};
