#include "Commands/SavedProperties.h"

#include "Object/Class.h"
#include "Object/Object.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyFlags.h"

#include <algorithm>
#include <format>
#include <memory>

std::vector<const FProperty*> FSavedProperties::GetAll(const WClass& Class)
{
	std::vector<const WClass*> Classes;
	for (const WClass* Current = &Class; Current != nullptr; Current = Current->GetSuperClass())
	{
		Classes.push_back(Current);
	}
	std::ranges::reverse(Classes);

	std::vector<const FProperty*> Properties;
	for (const WClass* const Current : Classes)
	{
		for (const std::unique_ptr<FProperty>& Property : Current->GetOwnProperties())
		{
			if (!Property->HasAnyFlags(EPropertyFlags::Transient))
			{
				Properties.push_back(Property.get());
			}
		}
	}
	return Properties;
}

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
