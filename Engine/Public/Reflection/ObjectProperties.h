#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>
#include <vector>

class FJsonValue;
class FProperty;
class IPropertyJsonContext;
class WClass;
class WObject;

/**
 * The properties that are saved with an object, and their values as JSON: what a scene file holds of each of its
 * objects, what an asset file holds of its asset, and what the editor shows and changes.
 *
 * A property is saved unless it is Transient. A Transient property holds what an object works out for itself or is
 * given by whoever owns it, such as an entity's place in the hierarchy, and is neither written nor read.
 */
class FObjectProperties
{
public:
	/** Returns the saved properties of a class: those of its base classes first, each class's in declaration order. */
	ENGINE_API static std::vector<const FProperty*> GetSaved(const WClass& Class);

	/**
	 * Returns the values of an object's saved properties as a JSON object: one member for each, in the order of
	 * GetSaved, with the values that are the class's defaults too.
	 *
	 * @param Context How references to other objects are written.
	 */
	ENGINE_API static FJsonValue Export(const WObject& Object, IPropertyJsonContext& Context);

	/**
	 * Gives an object the values of a JSON object of saved properties, as Export writes them. A property that the JSON
	 * does not name keeps the value it has. A member that cannot be used is a problem: its property keeps the value it
	 * has, and the other members are read all the same.
	 *
	 * @param Properties A JSON object. Anything else is a fatal error: a document's structure is its reader's to check.
	 * @param Context How references to other objects are found.
	 * @param SavedWith What the object is saved with, for the sentence about a Transient property: "a scene".
	 * @param OutProblems A sentence is added for each member that could not be used, in the order of the members:
	 * 'there is no property named "Colour"; the value was ignored', 'the property "Package" is not saved with a
	 * material; the value was ignored', and 'property "Roughness": ' followed by why the value was refused.
	 */
	ENGINE_API static void Import(WObject& Object,
		const FJsonValue& Properties,
		IPropertyJsonContext& Context,
		std::string_view SavedWith,
		std::vector<std::string>& OutProblems);
};
