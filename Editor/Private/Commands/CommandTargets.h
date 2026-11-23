#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

class FEditor;
class FJsonValue;
class WComponent;
class WEntity;
class WObject;

/**
 * Finds what the arguments of commands refer to: the entities and components of the editor's scene, by their
 * identifiers. Each function returns null and says why in OutError if there is no such object.
 */
class FCommandTargets
{
public:
	/** Returns the entity of the editor's scene with the given identifier. */
	static WEntity* FindEntity(const FEditor& Editor, int64 EntityId, std::string& OutError);

	/** Returns the component of the editor's scene with the given identifier. */
	static WComponent* FindComponent(const FEditor& Editor, int64 ComponentId, std::string& OutError);

	/** Returns the entity or component of the editor's scene with the given identifier. */
	static WObject* FindObject(const FEditor& Editor, int64 ObjectId, std::string& OutError);

	/**
	 * Reads an argument that is a list of entities: a JSON array of the identifiers of entities of the editor's
	 * scene.
	 *
	 * @param Entities The argument, which commands call "Entities".
	 * @param OutEntityIds Receives the identifiers, in the order they were given.
	 * @return False, with the reason in OutError, if the argument is not an array, holds something that is not an
	 * integer, or names an entity that the scene does not have.
	 */
	static bool ReadEntityIds(
		const FEditor& Editor, const FJsonValue& Entities, std::vector<int64>& OutEntityIds, std::string& OutError);
};
