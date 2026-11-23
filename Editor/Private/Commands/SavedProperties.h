#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

class FProperty;
class WClass;
class WObject;

/**
 * The properties that the editor reads and changes: those that are saved with a scene, which are the ones the
 * commands Object.GetProperty and Object.SetProperty work on and the ones the Details panel shows.
 *
 * A Transient property holds what an object works out for itself, such as an entity's place in the hierarchy, and
 * changing one behind the object's back would break it; the hierarchy has commands of its own.
 */
class FSavedProperties
{
public:
	/** Returns the saved properties of a class, those of its base classes first, each in declaration order. */
	static std::vector<const FProperty*> GetAll(const WClass& Class);

	/**
	 * Returns the saved property of that name of Object's class.
	 *
	 * @param OutError Receives a sentence that says what is wrong if there is none.
	 * @return The property, or null if the class has no property of that name or it is not saved.
	 */
	static const FProperty* Find(const WObject& Object, const std::string& PropertyName, std::string& OutError);
};
