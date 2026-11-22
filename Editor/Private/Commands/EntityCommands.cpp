#include "Commands/EntityCommands.h"

#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TransformRule.h"

#include <format>
#include <string>
#include <utility>

namespace
{
	FCommandResult CreateEntity(FEditor& Editor, const FCommandArguments& Arguments)
	{
		WEntity* Parent = nullptr;
		if (Arguments.Has("Parent"))
		{
			std::string Error;
			Parent = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Parent"), Error);
			if (Parent == nullptr)
			{
				return FCommandResult::Failure(std::move(Error));
			}
		}

		const WEntity* const Entity = Editor.GetScene().CreateEntity(Arguments.GetString("Name"), Parent);
		if (Entity == nullptr)
		{
			return FCommandResult::Failure("The scene refused to create the entity; the log says why.");
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Id", Entity->GetId());
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult DeleteEntity(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WEntity* const Entity = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Entity"), Error);
		if (Entity == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		Entity->Destroy();
		return FCommandResult::Success();
	}

	FCommandResult RenameEntity(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WEntity* const Entity = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Entity"), Error);
		if (Entity == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		Entity->SetName(Arguments.GetString("Name"));
		return FCommandResult::Success();
	}

	FCommandResult SetEntityParent(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WEntity* const Entity = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Entity"), Error);
		if (Entity == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		WEntity* NewParent = nullptr;
		if (Arguments.Has("Parent"))
		{
			NewParent = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Parent"), Error);
			if (NewParent == nullptr)
			{
				return FCommandResult::Failure(std::move(Error));
			}
			if (NewParent == Entity || NewParent->IsDescendantOf(Entity))
			{
				return FCommandResult::Failure(
					std::format("The entity {} cannot become a child of the entity {}, which is the entity itself or "
								"one of its descendants.",
						Entity->GetId(),
						NewParent->GetId()));
			}
		}

		const bool bKeepWorldTransform =
			!Arguments.Has("KeepWorldTransform") || Arguments.GetBoolean("KeepWorldTransform");
		const ETransformRule TransformRule =
			bKeepWorldTransform ? ETransformRule::KeepWorld : ETransformRule::KeepRelative;
		if (!Entity->SetParent(NewParent, TransformRule))
		{
			return FCommandResult::Failure("The scene refused to give the entity that parent; the log says why.");
		}
		return FCommandResult::Success();
	}
}

void FEntityCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Entity.Create",
			"Creates an entity in the scene being edited. Returns its identifier.",
			{FCommandParameter::Required(
				 "Name", ECommandArgumentType::String, "The entity's name. Names need not be unique."),
				FCommandParameter::Optional("Parent",
					ECommandArgumentType::Integer,
					"The identifier of the entity to attach it to. At the root of the scene if left out.")}},
		CreateEntity);

	Registry.Register(
		{"Entity.Delete",
			"Destroys an entity together with its components and all its descendants.",
			{FCommandParameter::Required("Entity", ECommandArgumentType::Integer, "The identifier of the entity.")}},
		DeleteEntity);

	Registry.Register(
		{"Entity.Rename",
			"Gives an entity another name.",
			{FCommandParameter::Required("Entity", ECommandArgumentType::Integer, "The identifier of the entity."),
				FCommandParameter::Required("Name", ECommandArgumentType::String, "The new name.")}},
		RenameEntity);

	Registry.Register(
		{"Entity.SetParent",
			"Gives an entity another parent, or none. The entity becomes the last child of its new parent.",
			{FCommandParameter::Required("Entity", ECommandArgumentType::Integer, "The identifier of the entity."),
				FCommandParameter::Optional("Parent",
					ECommandArgumentType::Integer,
					"The identifier of the new parent. The entity moves to the root of the scene if left out."),
				FCommandParameter::Optional("KeepWorldTransform",
					ECommandArgumentType::Boolean,
					"True, the default, to leave the entity where it is in the world. False to keep its transform "
					"relative to its parent, so that it moves in the world.")}},
		SetEntityParent);
}
