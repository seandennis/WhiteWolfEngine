#pragma once

#include "CoreMinimal.h"

#include <string>

class FEditor;
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
};
