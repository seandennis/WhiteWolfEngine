#include "Commands/ComponentCommands.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"

#include <format>
#include <string>
#include <utility>

namespace
{
	FCommandResult AddComponent(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WEntity* const Entity = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Entity"), Error);
		if (Entity == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		const std::string& ClassName = Arguments.GetString("Class");
		WClass* const Class = WClass::FindByName(ClassName);
		if (Class == nullptr)
		{
			return FCommandResult::Failure(std::format("There is no class \"{}\".", ClassName));
		}
		if (!Class->IsChildOf(WComponent::StaticClass()))
		{
			return FCommandResult::Failure(std::format("The class {} is not a component class.", ClassName));
		}
		if (Class->IsAbstract())
		{
			return FCommandResult::Failure(
				std::format("The class {} is abstract: there can be no component of that class itself.", ClassName));
		}

		const WComponent* const Component = Entity->AddComponent(Class);
		if (Component == nullptr)
		{
			return FCommandResult::Failure("The entity refused the component; the log says why.");
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Id", Component->GetId());
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult RemoveComponent(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WComponent* const Component = FCommandTargets::FindComponent(Editor, Arguments.GetInteger("Component"), Error);
		if (Component == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		Component->Destroy();
		return FCommandResult::Success();
	}
}

void FComponentCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Component.Add",
			"Creates a component and adds it to an entity. Returns the component's identifier.",
			{FCommandParameter::Required("Entity", ECommandArgumentType::Integer, "The identifier of the entity."),
				FCommandParameter::Required("Class",
					ECommandArgumentType::String,
					"The name of the component's class, for example \"WPointLightComponent\".")}},
		AddComponent);

	Registry.Register({"Component.Remove",
						  "Removes a component from its entity and destroys it.",
						  {FCommandParameter::Required(
							  "Component", ECommandArgumentType::Integer, "The identifier of the component.")}},
		RemoveComponent);
}
