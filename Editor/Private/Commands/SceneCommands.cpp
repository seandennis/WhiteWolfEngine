#include "Commands/SceneCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <filesystem>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace
{
	/** Returns an entity with its components and, below it, its children, as Scene.GetHierarchy shows them. */
	FJsonValue DescribeEntity(const WEntity& Entity)
	{
		FJsonValue ComponentList = FJsonValue::MakeArray();
		for (const TObjectPtr<WComponent>& Component : Entity.GetComponents())
		{
			FJsonValue ComponentJson = FJsonValue::MakeObject();
			ComponentJson.SetMember("Id", Component->GetId());
			// A component whose class this build does not have is shown as what it was saved as.
			if (const WMissingComponent* const Missing = Cast<WMissingComponent>(Component.Get()))
			{
				ComponentJson.SetMember("Class", Missing->GetMissingClassName());
				ComponentJson.SetMember("Missing", true);
			}
			else
			{
				ComponentJson.SetMember("Class", Component->GetClass()->GetName());
			}
			ComponentList.AddElement(std::move(ComponentJson));
		}

		FJsonValue ChildList = FJsonValue::MakeArray();
		for (const TObjectPtr<WEntity>& Child : Entity.GetChildren())
		{
			ChildList.AddElement(DescribeEntity(*Child));
		}

		FJsonValue EntityJson = FJsonValue::MakeObject();
		EntityJson.SetMember("Id", Entity.GetId());
		EntityJson.SetMember("Name", Entity.GetName());
		EntityJson.SetMember("Components", std::move(ComponentList));
		EntityJson.SetMember("Children", std::move(ChildList));
		return EntityJson;
	}

	FCommandResult NewScene(FEditor& Editor, const FCommandArguments& Arguments)
	{
		WScene* const Scene = NewObject<WScene>();
		Scene->SetName(Arguments.Has("Name") ? Arguments.GetString("Name") : std::string(FEditor::DefaultSceneName));
		Editor.SetScene(*Scene, std::filesystem::path());
		return FCommandResult::Success();
	}

	FCommandResult OpenScene(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::filesystem::path Path = Editor.ResolveFilePath(FFileHelper::PathFromUtf8(Arguments.GetString("Path")));

		std::string Error;
		std::vector<std::string> Warnings;
		WScene* const Scene = FSceneSerializer::LoadFromFile(Path, Error, &Warnings);
		if (Scene == nullptr)
		{
			return FCommandResult::Failure(std::format("The scene was not opened: {}.", Error));
		}
		Editor.SetScene(*Scene, std::move(Path));

		FJsonValue WarningList = FJsonValue::MakeArray();
		for (std::string& Warning : Warnings)
		{
			WarningList.AddElement(std::move(Warning));
		}
		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Warnings", std::move(WarningList));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult SaveScene(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::filesystem::path Path = Arguments.Has("Path")
			? Editor.ResolveFilePath(FFileHelper::PathFromUtf8(Arguments.GetString("Path")))
			: Editor.GetScenePath();
		if (Path.empty())
		{
			return FCommandResult::Failure(
				"The scene has no file yet. Give the argument \"Path\" to say where to save it.");
		}

		std::string Error;
		if (!FSceneSerializer::SaveToFile(Editor.GetScene(), Path, Error))
		{
			return FCommandResult::Failure(std::format("The scene was not saved: {}.", Error));
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Path", FFileHelper::PathToUtf8(Path));
		Editor.SetScenePath(std::move(Path));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult GetHierarchy(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		const WScene& Scene = Editor.GetScene();

		FJsonValue EntityList = FJsonValue::MakeArray();
		for (const TObjectPtr<WEntity>& Entity : Scene.GetRootEntities())
		{
			EntityList.AddElement(DescribeEntity(*Entity));
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Name", Scene.GetName());
		Value.SetMember("Path", FFileHelper::PathToUtf8(Editor.GetScenePath()));
		Value.SetMember("Entities", std::move(EntityList));
		return FCommandResult::Success(std::move(Value));
	}
}

void FSceneCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Scene.New",
			"Replaces the scene being edited with an empty one. What was not saved of the old scene is lost, and "
			"Edit.Undo does not bring it back.",
			{FCommandParameter::Optional(
				"Name", ECommandArgumentType::String, "The name of the new scene. \"Untitled\" if left out.")}},
		NewScene);

	Registry.Register(
		{"Scene.Open",
			"Replaces the scene being edited with one loaded from a scene file. What was not saved of the old scene "
			"is lost, and Edit.Undo does not bring it back. Returns the warnings about values in the file that could "
			"not be loaded.",
			{FCommandParameter::Required("Path",
				ECommandArgumentType::String,
				"The scene file to load. A relative path is relative to the content directory of the project, if one "
				"is open.")}},
		OpenScene);

	Registry.Register(
		{"Scene.Save",
			"Saves the scene being edited to a scene file, which becomes the scene's file. Returns its path.",
			{FCommandParameter::Optional("Path",
				ECommandArgumentType::String,
				"The file to save to; a relative path is relative to the content directory of the project, if one is "
				"open. The file the scene was opened from or last saved to if left out.")},
			ECommandEffect::LeavesScene},
		SaveScene);

	Registry.Register(
		{"Scene.GetHierarchy",
			"Returns the name and file of the scene being edited and the tree of its entities: each with its "
			"identifier, name, components (identifier and class) and children.",
			{},
			ECommandEffect::LeavesScene},
		GetHierarchy);
}
