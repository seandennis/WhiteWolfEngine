#include "Commands/PropertyCommands.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Commands/ScenePropertyContext.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/Object.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyFlags.h"

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Commands read and change the properties that are saved with a scene, which are also the ones the Details panel
// shows. A Transient property holds what an object works out for itself, such as an entity's place in the
// hierarchy, and changing one behind the object's back would break it; the hierarchy has commands of its own.

namespace
{
	/** Returns the properties of a class that commands read and change, those of its base classes first. */
	std::vector<const FProperty*> GetSavedProperties(const WClass& Class)
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

	/** Returns the property of that name of Object's class, or null, with the reason, if commands cannot use it. */
	const FProperty* FindSavedProperty(const WObject& Object, const std::string& PropertyName, std::string& OutError)
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

	FCommandResult GetProperties(FEditor& Editor, const FCommandArguments& Arguments)
	{
		const int64 ObjectId = Arguments.GetInteger("Object");
		std::string Error;
		const WObject* const Object = FCommandTargets::FindObject(Editor, ObjectId, Error);
		if (Object == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		FScenePropertyContext Context(Editor.GetScene());
		FJsonValue PropertyValues = FJsonValue::MakeObject();
		for (const FProperty* const Property : GetSavedProperties(*Object->GetClass()))
		{
			PropertyValues.SetMember(Property->GetName(), Property->ExportJson(Property->GetValuePtr(Object), Context));
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Id", ObjectId);
		Value.SetMember("Class", Object->GetClass()->GetName());
		Value.SetMember("Properties", std::move(PropertyValues));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult GetProperty(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		const WObject* const Object = FCommandTargets::FindObject(Editor, Arguments.GetInteger("Object"), Error);
		if (Object == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}
		const FProperty* const Property = FindSavedProperty(*Object, Arguments.GetString("Property"), Error);
		if (Property == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		FScenePropertyContext Context(Editor.GetScene());
		return FCommandResult::Success(Property->ExportJson(Property->GetValuePtr(Object), Context));
	}

	FCommandResult SetProperty(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WObject* const Object = FCommandTargets::FindObject(Editor, Arguments.GetInteger("Object"), Error);
		if (Object == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}
		const std::string& PropertyName = Arguments.GetString("Property");
		const FProperty* const Property = FindSavedProperty(*Object, PropertyName, Error);
		if (Property == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		FScenePropertyContext Context(Editor.GetScene());
		if (!Property->ImportJson(Property->GetValuePtr(Object), Arguments.GetJson("Value"), Context, Error))
		{
			return FCommandResult::Failure(std::format("The property \"{}\" of the class {} was not changed: {}.",
				PropertyName,
				Object->GetClass()->GetName(),
				Error));
		}
		return FCommandResult::Success();
	}
}

void FPropertyCommands::Register(FCommandRegistry& Registry)
{
	const FCommandParameter ObjectParameter = FCommandParameter::Required(
		"Object", ECommandArgumentType::Integer, "The identifier of an entity or of a component.");

	Registry.Register(
		{"Object.GetProperties",
			"Returns the class of an entity or component and the value of every property that is saved with the "
			"scene. A reference to an entity or component is its identifier.",
			{ObjectParameter}},
		GetProperties);

	Registry.Register(
		{"Object.GetProperty",
			"Returns the value of one property of an entity or component.",
			{ObjectParameter,
				FCommandParameter::Required("Property", ECommandArgumentType::String, "The name of the property.")}},
		GetProperty);

	Registry.Register(
		{"Object.SetProperty",
			"Changes the value of one property of an entity or component.",
			{ObjectParameter,
				FCommandParameter::Required("Property", ECommandArgumentType::String, "The name of the property."),
				FCommandParameter::Required("Value",
					ECommandArgumentType::Json,
					"The new value, in the form Object.GetProperty returns it. A reference to an entity or "
					"component is its identifier, and null is no reference.")}},
		SetProperty);
}
