#include "Commands/PropertyCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Commands/SavedProperties.h"
#include "Commands/ScenePropertyContext.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/Object.h"
#include "Reflection/Property.h"

#include <format>
#include <string>
#include <utility>

namespace
{
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
		for (const FProperty* const Property : FSavedProperties::GetAll(*Object->GetClass()))
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
		const FProperty* const Property = FSavedProperties::Find(*Object, Arguments.GetString("Property"), Error);
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
		const FProperty* const Property = FSavedProperties::Find(*Object, PropertyName, Error);
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
			{ObjectParameter},
			ECommandEffect::LeavesScene},
		GetProperties);

	Registry.Register(
		{"Object.GetProperty",
			"Returns the value of one property of an entity or component.",
			{ObjectParameter,
				FCommandParameter::Required("Property", ECommandArgumentType::String, "The name of the property.")},
			ECommandEffect::LeavesScene},
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
