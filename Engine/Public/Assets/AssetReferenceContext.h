#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"
#include "Reflection/PropertyJsonContext.h"

#include <string>
#include <vector>

/**
 * How a document that knows assets and nothing else writes and finds the objects that its properties refer to, as
 * a material file does: an asset is written as its asset path (FAssetPath), and a path is found by loading it
 * (FAssetManager::Load). A number, as a scene writes an entity or a component, is refused: it means one scene.
 */
class ENGINE_API FAssetReferenceContext final : public IPropertyJsonContext
{
public:
	/**
	 * Returns the asset path of an asset that has one. Anything else, as an asset that was made in code, an entity
	 * or a component, cannot be written: it is JSON null, and the name of its class is added to GetUnwritten, for
	 * whoever writes the document to say so. Nothing is logged here, so that only reading values never warns.
	 */
	FJsonValue ExportObjectReference(const WObject& Object) override;

	/**
	 * Finds the asset of an asset path, which is loaded if it is not loaded yet. The reason that the asset manager
	 * gives for a path it cannot load is passed on as it is; anything but a string is refused.
	 */
	bool ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError) override;

	/** Returns the names of the classes of the objects that could not be written, in the order they were asked. */
	const std::vector<std::string>& GetUnwritten() const
	{
		return Unwritten;
	}

private:
	std::vector<std::string> Unwritten;
};
