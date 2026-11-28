#include "Commands/SavedProperties.h"

#include "Object/Class.h"
#include "Object/Object.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyFlags.h"

#include <format>

const FProperty* FSavedProperties::Find(const WObject& Object, const std::string& PropertyName, std::string& OutError)
{
	const WClass& Class = *Object.GetClass();
	const FProperty* const Property = Class.FindProperty(PropertyName);
	if (Property == nullptr)
	{
		OutError = std::format("The class {} has no property \"{}\".", Class.GetName(), PropertyName);
		return nullptr;
	}
	if (Property->HasAnyFlags(EPropertyFlags::Transient))
	{
		OutError = std::format("The property \"{}\" of the class {} is not saved with the scene, and commands "
							   "neither read nor change such properties.",
			PropertyName,
			Class.GetName());
		return nullptr;
	}
	return Property;
}
