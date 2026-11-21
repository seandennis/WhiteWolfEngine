#pragma once

#include "CoreMinimal.h"

#include <string>

class FJsonValue;
class IPropertyJsonContext;
class WClass;
class WObject;

/** The JSON form of a reference to an object, shared by the strong and the weak object property. */
class FObjectReferenceJson
{
public:
	/** Returns JSON null for a null or destroyed object, and otherwise what Context writes for the object. */
	static FJsonValue Export(const WObject* Object, IPropertyJsonContext& Context);

	/**
	 * Finds the object that Json refers to. JSON null gives a null object.
	 *
	 * @param RequiredClass The class the object must be an instance of.
	 * @return False, with the reason in OutError, if Context cannot find the object or it is of another class.
	 */
	static bool Import(const FJsonValue& Json,
		const WClass& RequiredClass,
		IPropertyJsonContext& Context,
		WObject*& OutObject,
		std::string& OutError);
};
