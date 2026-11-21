#pragma once

#include "CoreMinimal.h"

#include <string>

class FJsonValue;
class WObject;

/**
 * What a property needs from whoever is saving or loading it: a way to write a reference to another object as JSON
 * and to find the object again.
 *
 * How an object is identified depends on the document. A scene identifies its entities and components by number; a
 * document that holds no objects can refuse every reference.
 */
class ENGINE_API IPropertyJsonContext
{
public:
	virtual ~IPropertyJsonContext() = default;

	/**
	 * Returns the JSON that identifies Object in the document being written. Returns JSON null if the document
	 * cannot refer to that object; the reference is then null when the document is loaded again.
	 */
	virtual FJsonValue ExportObjectReference(const WObject& Object) = 0;

	/**
	 * Finds the object that Json, which is not JSON null, identifies.
	 *
	 * @param OutObject Receives the object. Unchanged on failure.
	 * @param OutError Receives the reason on failure.
	 * @return True if the object was found.
	 */
	virtual bool ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError) = 0;
};
