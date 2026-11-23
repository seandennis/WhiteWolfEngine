#include "Commands/ComponentCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Commands/ComponentClasses.h"
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
		if (!FComponentClasses::CanBeAdded(*Class, Error))
		{
			return FCommandResult::Failure(std::move(Error));
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

	FCommandResult ListClasses(FEditor& /*Editor*/, const FCommandArguments& /*Arguments*/)
	{
		FJsonValue Classes = FJsonValue::MakeArray();
		for (const WClass* const Class : FComponentClasses::GetAddable())
		{
			Classes.AddElement(Class->GetName());
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Classes", std::move(Classes));
		return FCommandResult::Success(std::move(Value));
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

	Registry.Register(
		{"Component.ListClasses",
			"Returns the names of the classes of components that Component.Add can give an entity, those of plugins "
			"that are loaded included, in the order of the names. What a component of a class holds is seen by "
			"adding one and asking for its properties with Object.GetProperties.",
			{},
			ECommandEffect::LeavesScene},
		ListClasses);
}
