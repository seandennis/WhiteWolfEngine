#pragma once

#include "CoreMinimal.h"

#include "Reflection/PropertyJsonContext.h"

#include <string>

class FJsonValue;
class WObject;
class WScene;

/**
 * How commands read and write the references that properties hold to other objects, which is how a scene file
 * writes them: an entity or a component of the scene is its identifier, and an asset of the content directory is its
 * asset path. A reference to anything else reads as JSON null.
 */
class FScenePropertyContext final : public IPropertyJsonContext
{
public:
	/** @param InScene The scene whose entities and components references may refer to. Must outlive the context. */
	explicit FScenePropertyContext(const WScene& InScene);

	FJsonValue ExportObjectReference(const WObject& Object) override;
	bool ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError) override;

private:
	const WScene& Scene;
};
