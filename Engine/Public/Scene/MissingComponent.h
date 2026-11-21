#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"
#include "Object/ObjectMacros.h"
#include "Scene/Component.h"

#include <string>

/**
 * Stands in for a component whose class does not exist in this build, for example because the plugin that
 * provides it is not enabled.
 *
 * When a scene is loaded and a component's class cannot be found, the component becomes one of these and keeps
 * the saved data untouched. Saving the scene writes that data back under the original class name, so opening and
 * saving a scene without one of its plugins loses nothing.
 */
WCLASS()
class ENGINE_API WMissingComponent : public WComponent
{
	WW_DECLARE_CLASS(WMissingComponent, WComponent)

public:
	/** Returns the name of the class the component was saved as. */
	const std::string& GetMissingClassName() const;

	/** Returns the component's saved properties, a JSON object, exactly as they were in the scene file. */
	const FJsonValue& GetSavedProperties() const;

private:
	friend class FSceneSerializer;

	std::string MissingClassName;
	FJsonValue SavedProperties = FJsonValue::MakeObject();
};
