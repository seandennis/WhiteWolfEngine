#pragma once

#include "CoreMinimal.h"

#include <string>

class FProperty;
class WObject;

/**
 * Finds a property that the editor reads and changes by its name, with the sentence that says why there is none. The
 * properties are those that are saved (FObjectProperties::GetSaved), which are the ones the commands
 * Object.GetProperty, Object.SetProperty and Asset.SetProperty work on and the ones the Details panel shows.
 *
 * A Transient property holds what an object works out for itself, such as an entity's place in the hierarchy, and
 * changing one behind the object's back would break it; the hierarchy has commands of its own.
 */
class FSavedProperties
{
public:
	/**
	 * Returns the saved property of that name of Object's class.
	 *
	 * @param OutError Receives a sentence that says what is wrong if there is none.
	 * @return The property, or null if the class has no property of that name or it is not saved.
	 */
	static const FProperty* Find(const WObject& Object, const std::string& PropertyName, std::string& OutError);
};
