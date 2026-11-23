#include "Ui/UiCommands.h"

#include "Commands/CommandResult.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Math/Transform.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/PropertyValueTraits.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"
#include "Ui/UiNewEntity.h"
#include "Ui/UiText.h"
#include "Viewport/ViewportCamera.h"

#include <glm/common.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	/**
	 * One thing that the user did, as a step of the history: begun where this is created and ended where it goes
	 * out of scope, whatever number of commands are given in between.
	 *
	 * A step that is still open belongs to something the user was in the middle of, such as dragging a number
	 * (FUiCommands::BeginChange). Doing something else ends that first, because steps do not nest.
	 */
	class FUserStep
	{
	public:
		FUserStep(FEditor& InEditor, std::string StepName)
			: Editor(InEditor)
		{
			Editor.EndStep();
			Editor.BeginStep(std::move(StepName));
		}

		~FUserStep()
		{
			Editor.EndStep();
		}

		FUserStep(const FUserStep&) = delete;
		FUserStep& operator=(const FUserStep&) = delete;

	private:
		FEditor& Editor;
	};

	/** Returns the arguments of a command that acts on one entity. */
	FJsonValue MakeEntityArguments(int64 EntityId)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Entity", EntityId);
		return Arguments;
	}

	/** The properties that the user interface sets by name: where an entity is, and what a mesh component shows. */
	constexpr std::string_view EntityTransformProperty = "RelativeTransform";
	constexpr std::string_view MeshProperty = "Mesh";

	/** The parts of a metre that the place of a new entity is rounded to. */
	constexpr double PlacesPerMetre = 100.0;

	/** Returns the arguments of Object.SetProperty. */
	FJsonValue MakePropertyArguments(int64 ObjectId, std::string_view PropertyName, const FJsonValue& Value)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Object", ObjectId);
		Arguments.SetMember("Property", PropertyName);
		Arguments.SetMember("Value", Value);
		return Arguments;
	}

	/**
	 * Gives the editor a command whose arguments the code itself has written, not the user: a refusal means that
	 * the code is wrong, and is a fatal error.
	 *
	 * @return What the command returned.
	 */
	FJsonValue ExecuteOwnCommand(FEditor& Editor, std::string_view Name, const FJsonValue& Arguments)
	{
		const FCommandResult Result = Editor.ExecuteCommand(Name, Arguments);
		WW_CHECKF(Result.HasSucceeded(),
			"The editor refused the command {}, which the user interface wrote itself: {}",
			Name,
			Result.GetError());
		return Result.GetValue();
	}

	/**
	 * Returns those of the entities that exist and are not below another of them, in the order they were given: the
	 * ones to act on when what is done to an entity is done to everything below it.
	 */
	std::vector<int64> GetTopmostEntities(const FEditor& Editor, const std::vector<int64>& EntityIds)
	{
		const WScene& Scene = Editor.GetScene();
		std::vector<int64> Topmost;
		for (const int64 EntityId : EntityIds)
		{
			const WEntity* const Entity = Scene.FindEntityById(EntityId);
			if (Entity == nullptr)
			{
				continue;
			}

			const bool bIsBelowAnother = std::ranges::any_of(EntityIds,
				[&Scene, Entity](int64 OtherId) { return Entity->IsDescendantOf(Scene.FindEntityById(OtherId)); });
			if (!bIsBelowAnother)
			{
				Topmost.push_back(EntityId);
			}
		}
		return Topmost;
	}
}

FCommandResult FUiCommands::NewProject(FEditor& Editor, const std::string& Directory, const std::string& Name)
{
	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Directory", Directory);
	Arguments.SetMember("Name", Name);
	return Editor.ExecuteCommand("Project.New", Arguments);
}

FCommandResult FUiCommands::OpenProject(FEditor& Editor, const std::string& Path)
{
	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Path", Path);
	return Editor.ExecuteCommand("Project.Open", Arguments);
}

void FUiCommands::NewScene(FEditor& Editor)
{
	Editor.ExecuteCommand("Scene.New", FJsonValue());
}

FCommandResult FUiCommands::SaveScene(FEditor& Editor)
{
	return Editor.ExecuteCommand("Scene.Save", FJsonValue());
}

FCommandResult FUiCommands::SaveSceneAs(FEditor& Editor, const std::string& Path)
{
	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Path", Path);
	return Editor.ExecuteCommand("Scene.Save", Arguments);
}

FCommandResult FUiCommands::OpenScene(FEditor& Editor, const std::string& Path)
{
	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Path", Path);
	return Editor.ExecuteCommand("Scene.Open", Arguments);
}

std::vector<std::string> FUiCommands::ListScenes(FEditor& Editor)
{
	std::vector<std::string> Scenes;
	// Without a project there are no scenes of one to list, which the command would say by failing.
	if (Editor.GetProject() == nullptr)
	{
		return Scenes;
	}

	const FCommandResult Result = Editor.ExecuteCommand("Scene.List", FJsonValue());
	if (Result.HasSucceeded())
	{
		for (const FJsonValue& Scene : Result.GetValue().GetElements())
		{
			Scenes.push_back(Scene.GetString());
		}
	}
	return Scenes;
}

void FUiCommands::Undo(FEditor& Editor)
{
	Editor.ExecuteCommand("Edit.Undo", FJsonValue());
}

void FUiCommands::Redo(FEditor& Editor)
{
	Editor.ExecuteCommand("Edit.Redo", FJsonValue());
}

void FUiCommands::SetSelection(FEditor& Editor, const std::vector<int64>& EntityIds)
{
	FJsonValue Entities = FJsonValue::MakeArray();
	for (const int64 EntityId : EntityIds)
	{
		Entities.AddElement(EntityId);
	}

	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Entities", std::move(Entities));
	Editor.ExecuteCommand("Selection.Set", Arguments);
}

std::optional<int64> FUiCommands::CreateEntity(FEditor& Editor, const std::string& Name, std::optional<int64> ParentId)
{
	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Name", Name);
	if (ParentId.has_value())
	{
		Arguments.SetMember("Parent", *ParentId);
	}

	const FUserStep Step(Editor, "Create Entity");
	const FCommandResult Result = Editor.ExecuteCommand("Entity.Create", Arguments);
	if (!Result.HasSucceeded())
	{
		return std::nullopt;
	}

	const int64 EntityId = Result.GetValue().FindMember("Id")->GetInteger();
	SetSelection(Editor, {EntityId});
	return EntityId;
}

void FUiCommands::RenameEntity(FEditor& Editor, int64 EntityId, const std::string& Name)
{
	FJsonValue Arguments = MakeEntityArguments(EntityId);
	Arguments.SetMember("Name", Name);

	const FUserStep Step(Editor, "Rename");
	Editor.ExecuteCommand("Entity.Rename", Arguments);
}

void FUiCommands::DeleteEntities(FEditor& Editor, const std::vector<int64>& EntityIds)
{
	const FUserStep Step(Editor, "Delete");
	for (const int64 EntityId : GetTopmostEntities(Editor, EntityIds))
	{
		Editor.ExecuteCommand("Entity.Delete", MakeEntityArguments(EntityId));
	}
}

void FUiCommands::DuplicateEntities(FEditor& Editor, const std::vector<int64>& EntityIds)
{
	const std::vector<int64> Originals = GetTopmostEntities(Editor, EntityIds);
	if (Originals.empty())
	{
		return;
	}

	const FUserStep Step(Editor, "Duplicate");
	std::vector<int64> Copies;
	for (const int64 EntityId : Originals)
	{
		const FCommandResult Result = Editor.ExecuteCommand("Entity.Duplicate", MakeEntityArguments(EntityId));
		if (Result.HasSucceeded())
		{
			Copies.push_back(Result.GetValue().FindMember("Id")->GetInteger());
		}
	}
	// The copies are what the user goes on to work with, as after a paste.
	SetSelection(Editor, Copies);
}

void FUiCommands::AttachEntities(FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId)
{
	const FUserStep Step(Editor, "Attach");
	for (const int64 EntityId : GetTopmostEntities(Editor, EntityIds))
	{
		FJsonValue Arguments = MakeEntityArguments(EntityId);
		if (ParentId.has_value())
		{
			Arguments.SetMember("Parent", *ParentId);
		}
		Editor.ExecuteCommand("Entity.SetParent", Arguments);
	}
}

bool FUiCommands::CanAttachEntities(
	const FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId)
{
	if (!ParentId.has_value())
	{
		return true;
	}

	const WScene& Scene = Editor.GetScene();
	const WEntity* const Parent = Scene.FindEntityById(*ParentId);
	if (Parent == nullptr)
	{
		return false;
	}
	return std::ranges::none_of(EntityIds,
		[&Scene, Parent, &ParentId](int64 EntityId)
		{ return EntityId == *ParentId || Parent->IsDescendantOf(Scene.FindEntityById(EntityId)); });
}

void FUiCommands::AddEntity(FEditor& Editor, const FUiNewEntity& NewEntity)
{
	const FUserStep Step(Editor, std::format("Add {}", NewEntity.Name));

	FJsonValue CreateArguments = FJsonValue::MakeObject();
	CreateArguments.SetMember("Name", NewEntity.Name);
	const int64 EntityId = ExecuteOwnCommand(Editor, "Entity.Create", CreateArguments).FindMember("Id")->GetInteger();

	// What the user looks at is where the new thing goes: there it is in sight, wherever the camera has been
	// flown to. To the centimetre, so that its place reads as a number someone might have typed.
	FTransform Transform;
	Transform.Location = glm::round(Editor.GetViewportCamera().GetFocusPoint() * PlacesPerMetre) / PlacesPerMetre;
	Transform.Rotation = NewEntity.Rotation.ToQuat();
	ExecuteOwnCommand(Editor,
		"Object.SetProperty",
		MakePropertyArguments(EntityId, EntityTransformProperty, TPropertyValueTraits<FTransform>::ToJson(Transform)));

	if (!NewEntity.ComponentClassName.empty())
	{
		FJsonValue AddArguments = MakeEntityArguments(EntityId);
		AddArguments.SetMember("Class", NewEntity.ComponentClassName);
		const int64 ComponentId =
			ExecuteOwnCommand(Editor, "Component.Add", AddArguments).FindMember("Id")->GetInteger();
		if (!NewEntity.MeshPath.empty())
		{
			ExecuteOwnCommand(Editor,
				"Object.SetProperty",
				MakePropertyArguments(ComponentId, MeshProperty, FJsonValue(NewEntity.MeshPath)));
		}
	}
	SetSelection(Editor, {EntityId});
}

void FUiCommands::FocusSelection(FEditor& Editor)
{
	// With nothing selected there is nothing to look at, which the command would say by failing.
	if (!Editor.GetSelection().empty())
	{
		Editor.ExecuteCommand("Viewport.Focus", FJsonValue());
	}
}

void FUiCommands::AddComponent(FEditor& Editor, int64 EntityId, const std::string& ClassName)
{
	FJsonValue Arguments = MakeEntityArguments(EntityId);
	Arguments.SetMember("Class", ClassName);

	const FUserStep Step(Editor, std::format("Add {}", FUiText::MakeClassLabel(ClassName)));
	Editor.ExecuteCommand("Component.Add", Arguments);
}

void FUiCommands::RemoveComponent(FEditor& Editor, int64 ComponentId)
{
	// The step is named after what is removed, which only the component itself knows.
	std::string StepName = "Remove Component";
	if (const WComponent* const Component = Editor.GetScene().FindComponentById(ComponentId))
	{
		const WMissingComponent* const Missing = Cast<WMissingComponent>(Component);
		const std::string& ClassName =
			Missing != nullptr ? Missing->GetMissingClassName() : Component->GetClass()->GetName();
		StepName = std::format("Remove {}", FUiText::MakeClassLabel(ClassName));
	}

	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Component", ComponentId);

	const FUserStep Step(Editor, std::move(StepName));
	Editor.ExecuteCommand("Component.Remove", Arguments);
}

void FUiCommands::BeginChange(FEditor& Editor, std::string StepName)
{
	// A change that was under way is over when the user begins another.
	Editor.EndStep();
	Editor.BeginStep(std::move(StepName));
}

void FUiCommands::EndChange(FEditor& Editor)
{
	Editor.EndStep();
}

void FUiCommands::SetProperty(FEditor& Editor, int64 ObjectId, const std::string& PropertyName, const FJsonValue& Value)
{
	Editor.ExecuteCommand("Object.SetProperty", MakePropertyArguments(ObjectId, PropertyName, Value));
}
