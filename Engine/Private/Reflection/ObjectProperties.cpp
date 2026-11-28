#include "Reflection/ObjectProperties.h"

#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/Object.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyFlags.h"
#include "Reflection/PropertyJsonContext.h"

#include <format>
#include <memory>
#include <ranges>

std::vector<const FProperty*> FObjectProperties::GetSaved(const WClass& Class)
{
	std::vector<const WClass*> Hierarchy;
	for (const WClass* Current = &Class; Current != nullptr; Current = Current->GetSuperClass())
	{
		Hierarchy.push_back(Current);
	}

	std::vector<const FProperty*> Properties;
	for (const WClass* const Current : Hierarchy | std::views::reverse)
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

FJsonValue FObjectProperties::Export(const WObject& Object, IPropertyJsonContext& Context)
{
	FJsonValue Json = FJsonValue::MakeObject();
	for (const FProperty* const Property : GetSaved(*Object.GetClass()))
	{
		Json.SetMember(Property->GetName(), Property->ExportJson(Property->GetValuePtr(&Object), Context));
	}
	return Json;
}

void FObjectProperties::Import(WObject& Object,
	const FJsonValue& Properties,
	IPropertyJsonContext& Context,
	std::string_view SavedWith,
	std::vector<std::string>& OutProblems)
{
	WW_CHECKF(Properties.IsObject(),
		"The saved properties of a {} have to be a JSON object, not {}",
		Object.GetClass()->GetName(),
		Properties.GetTypeDescription());

	for (const FJsonValue::FMember& Member : Properties.GetMembers())
	{
		const FProperty* const Property = Object.GetClass()->FindProperty(Member.first);
		if (Property == nullptr)
		{
			OutProblems.push_back(
				std::format("there is no property named \"{}\"; the value was ignored", Member.first));
			continue;
		}
		if (Property->HasAnyFlags(EPropertyFlags::Transient))
		{
			OutProblems.push_back(std::format(
				"the property \"{}\" is not saved with {}; the value was ignored", Member.first, SavedWith));
			continue;
		}

		std::string Error;
		if (!Property->ImportJson(Property->GetValuePtr(&Object), Member.second, Context, Error))
		{
			OutProblems.push_back(std::format("property \"{}\": {}", Member.first, Error));
		}
	}
}
