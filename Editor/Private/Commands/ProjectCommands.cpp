#include "Commands/ProjectCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Project/Project.h"

#include <format>
#include <optional>
#include <string>
#include <utility>

namespace
{
	/** Returns what the project commands return about a project: its name and where it is. */
	FJsonValue DescribeProject(const FProject& Project)
	{
		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Name", Project.GetName());
		Value.SetMember("File", FFileHelper::PathToUtf8(Project.GetFile()));
		Value.SetMember("Directory", FFileHelper::PathToUtf8(Project.GetDirectory()));
		Value.SetMember("ContentDirectory", FFileHelper::PathToUtf8(Project.GetContentDirectory()));
		return Value;
	}

	FCommandResult NewProject(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		std::optional<FProject> Project = FProject::Create(
			FFileHelper::PathFromUtf8(Arguments.GetString("Directory")), Arguments.GetString("Name"), Error);
		if (!Project.has_value())
		{
			return FCommandResult::Failure(std::format("The project was not created: {}.", Error));
		}

		FJsonValue Value = DescribeProject(*Project);
		Editor.SetProject(std::move(*Project));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult OpenProject(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		std::optional<FProject> Project = FProject::Open(FFileHelper::PathFromUtf8(Arguments.GetString("Path")), Error);
		if (!Project.has_value())
		{
			return FCommandResult::Failure(std::format("The project was not opened: {}.", Error));
		}

		FJsonValue Value = DescribeProject(*Project);
		Editor.SetProject(std::move(*Project));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult GetProject(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		const FProject* const Project = Editor.GetProject();
		if (Project == nullptr)
		{
			return FCommandResult::Failure(
				"No project is open. Create one with Project.New or open one with Project.Open.");
		}
		return FCommandResult::Success(DescribeProject(*Project));
	}
}

void FProjectCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Project.New",
			"Creates a game project, a directory with a project file and a Content directory for the game's assets "
			"and scenes, and makes it the project being worked on. The scene being edited is replaced by an empty "
			"one. Returns the project's name, file, directory and content directory.",
			{FCommandParameter::Required("Directory",
				 ECommandArgumentType::String,
				 "Where the project is to be. The directory is created if it does not exist; it must not hold a "
				 "project."),
				FCommandParameter::Required("Name",
					ECommandArgumentType::String,
					"The project's name, which also names its file: letters, digits, spaces, \"-\" and \"_\".")}},
		NewProject);

	Registry.Register(
		{"Project.Open",
			"Makes an existing game project the one being worked on: asset paths are from now on relative to its "
			"Content directory. The scene being edited is replaced by an empty one. Returns the project's name, file, "
			"directory and content directory.",
			{FCommandParameter::Required("Path",
				ECommandArgumentType::String,
				"The project file (.wwproject), or the project's directory if it holds one project file.")}},
		OpenProject);

	Registry.Register(
		{"Project.Get",
			"Returns the name, file, directory and content directory of the project being worked on. Fails if no "
			"project is open.",
			{},
			ECommandEffect::LeavesScene},
		GetProject);
}
