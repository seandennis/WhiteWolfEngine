#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"

#include <unordered_map>

class FProperty;
class WClass;
class WScene;

/**
 * The values that the properties of a class have in an object that has just been created: what the Details panel
 * compares a value with to say that it was changed, and what its arrow puts back.
 *
 * The values of a class are found once, by creating an object of the class and writing its properties down, and
 * kept as JSON, the form commands take values in. The object is left to the garbage collector.
 */
class FPropertyDefaults
{
public:
	/**
	 * Returns the value that a property has in a new object of a class.
	 *
	 * @param Class The class of the object that is shown, which may give a property of a base class a value of its
	 * own.
	 * @param Property A saved property of that class (FSavedProperties).
	 * @param Scene The scene being edited, for writing down a reference that a new object might hold.
	 * @return The value, or null if no object of the class can be created.
	 */
	const FJsonValue* Find(WClass& Class, const FProperty& Property, const WScene& Scene);

private:
	using FValues = std::unordered_map<const FProperty*, FJsonValue>;

	/** The values of every class that was asked about. A class that no object can be made of has none. */
	std::unordered_map<const WClass*, FValues> ValuesByClass;
};
