#include "Commands/AssetCommands.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPackage.h"
#include "Assets/AssetPath.h"
#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/GltfImporter.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TransformRule.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	constexpr std::string_view NoProjectError = "No project is open, and assets belong to a project. Create one with "
												"Project.New or open one with Project.Open.";

	/** Returns lines of text as a JSON array of strings. */
	FJsonValue MakeStringList(const std::vector<std::string>& Lines)
	{
		FJsonValue List = FJsonValue::MakeArray();
		for (const std::string& Line : Lines)
		{
			List.AddElement(Line);
		}
		return List;
	}

	/**
	 * Loads an asset file of the content directory and says what is in it: the path and class of each asset, and
	 * what was left out when the file was loaded.
	 *
	 * @param OutValue Receives {"File": ..., "Assets": [{"Path": ..., "Class": ...}], "Warnings": [...]}.
	 * @return False, with the reason in OutError, if the file cannot be loaded.
	 */
	bool DescribeAssetFile(const std::string& File, FJsonValue& OutValue, std::string& OutError)
	{
		const WAssetPackage* const Package = FAssetManager::LoadPackage(File, OutError);
		if (Package == nullptr)
		{
			return false;
		}
		// A file that is one asset, a picture or an environment, is read now, to see that it can be.
		const bool bIsOneAsset =
			!FGltfImporter::IsGltfFile(FFileHelper::PathFromUtf8(File)) && !FAssetManager::IsBuiltInFile(File);
		if (bIsOneAsset && FAssetManager::Load(File, OutError) == nullptr)
		{
			return false;
		}

		FJsonValue Assets = FJsonValue::MakeArray();
		const auto AddAsset = [&Assets](const std::string& Path, const std::string& ClassName)
		{
			FJsonValue Asset = FJsonValue::MakeObject();
			Asset.SetMember("Path", Path);
			Asset.SetMember("Class", ClassName);
			Assets.AddElement(std::move(Asset));
		};
		for (const TObjectPtr<WAsset>& Asset : Package->GetAssets())
		{
			AddAsset(Asset->GetAssetPath(), Asset->GetClass()->GetName());
		}
		// A picture can also be had as a data texture, which exists only once it has been asked for.
		const bool bIsPicture = Cast<WTexture>(Package->FindAsset("")) != nullptr;
		if (bIsPicture && Package->FindAsset(FAssetManager::LinearTextureKey) == nullptr)
		{
			AddAsset(FAssetPath{File, std::string(FAssetManager::LinearTextureKey)}.ToString(),
				WTexture::StaticClass()->GetName());
		}

		OutValue = FJsonValue::MakeObject();
		OutValue.SetMember("File", File);
		OutValue.SetMember("Assets", std::move(Assets));
		OutValue.SetMember("Warnings", MakeStringList(Package->GetWarnings()));
		return true;
	}

	/** Removes files that an import copied and then could not use. What cannot be removed is left. */
	void RemoveFiles(const std::vector<std::filesystem::path>& Files)
	{
		for (const std::filesystem::path& File : Files)
		{
			std::error_code ErrorCode;
			std::filesystem::remove(File, ErrorCode);
		}
	}

	FCommandResult ImportAsset(FEditor& Editor, const FCommandArguments& Arguments)
	{
		if (Editor.GetProject() == nullptr)
		{
			return FCommandResult::Failure(std::string(NoProjectError));
		}

		const std::filesystem::path Source = FFileHelper::PathFromUtf8(Arguments.GetString("Source"));
		const std::string SourceText = FFileHelper::PathToUtf8(Source);
		std::error_code ErrorCode;
		if (!std::filesystem::is_regular_file(Source, ErrorCode))
		{
			return FCommandResult::Failure(std::format("There is no file '{}'.", SourceText));
		}
		if (!FAssetManager::IsAssetFile(Source))
		{
			return FCommandResult::Failure(std::format(
				"'{}' is not a kind of file that can be imported: a model is a .gltf or .glb file, a picture "
				"a .png, .jpg or .jpeg file, and an environment a .hdr file.",
				SourceText));
		}

		// Where the file goes: into the folder that was asked for, under its own name.
		const std::string FileName = FFileHelper::PathToUtf8(Source.filename());
		const std::string Folder = Arguments.Has("Folder") ? Arguments.GetString("Folder") : std::string();
		std::string Error;
		const std::optional<FAssetPath> Destination =
			FAssetPath::Parse(Folder.empty() ? FileName : Folder + "/" + FileName, Error);
		if (!Destination.has_value() || !Destination->Key.empty())
		{
			return FCommandResult::Failure(std::format(
				"The file cannot be imported into the folder \"{}\" as \"{}\": that is not a name a file of "
				"the content directory can have.",
				Folder,
				FileName));
		}

		// A model in a .gltf file keeps its data in files beside it, which have to come along.
		std::vector<std::string> ExternalFiles;
		if (FGltfImporter::IsGltfFile(Source) && !FGltfImporter::GetExternalFiles(Source, ExternalFiles, Error))
		{
			return FCommandResult::Failure(std::format("The file was not imported: {}.", Error));
		}

		struct FCopy
		{
			std::filesystem::path From;
			std::filesystem::path To;
		};
		const std::filesystem::path DestinationPath = FAssetManager::GetPathOfAssetFile(Destination->File);
		std::vector<FCopy> Copies = {{Source, DestinationPath}};
		for (const std::string& ExternalFile : ExternalFiles)
		{
			// Named like an asset file: relative, and not leading out of the directory of the model.
			const std::optional<FAssetPath> Relative = FAssetPath::Parse(ExternalFile, Error);
			if (!Relative.has_value() || !Relative->Key.empty())
			{
				return FCommandResult::Failure(
					std::format("The file was not imported: it keeps data in \"{}\", which is not beside or below it.",
						ExternalFile));
			}
			const std::filesystem::path RelativePath = FFileHelper::PathFromUtf8(Relative->File);
			Copies.push_back({Source.parent_path() / RelativePath, DestinationPath.parent_path() / RelativePath});
		}

		for (const FCopy& Copy : Copies)
		{
			if (std::filesystem::exists(Copy.To, ErrorCode))
			{
				return FCommandResult::Failure(
					std::format("The file was not imported: the project already has a file '{}'.",
						FFileHelper::PathToUtf8(Copy.To)));
			}
		}

		std::vector<std::filesystem::path> Copied;
		for (const FCopy& Copy : Copies)
		{
			std::filesystem::create_directories(Copy.To.parent_path(), ErrorCode);
			if (!ErrorCode)
			{
				std::filesystem::copy_file(Copy.From, Copy.To, ErrorCode);
			}
			if (ErrorCode)
			{
				RemoveFiles(Copied);
				return FCommandResult::Failure(
					std::format("The file was not imported: '{}' could not be copied to '{}': {}.",
						FFileHelper::PathToUtf8(Copy.From),
						FFileHelper::PathToUtf8(Copy.To),
						FFileHelper::DescribeError(ErrorCode)));
			}
			Copied.push_back(Copy.To);
		}

		FJsonValue Value;
		if (!DescribeAssetFile(Destination->File, Value, Error))
		{
			// A file that cannot be loaded is no asset, and is not left in the project.
			RemoveFiles(Copied);
			return FCommandResult::Failure(std::format("The file was not imported: {}.", Error));
		}
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult ListAssets(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		// What the engine has built in is there with or without a project.
		std::vector<std::string> Files = FAssetManager::GetBuiltInFiles();

		const FProject* const Project = Editor.GetProject();
		if (Project != nullptr)
		{
			std::error_code ErrorCode;
			for (std::filesystem::recursive_directory_iterator Entry(Project->GetContentDirectory(), ErrorCode);
				 !ErrorCode && Entry != std::filesystem::recursive_directory_iterator();
				 Entry.increment(ErrorCode))
			{
				std::error_code EntryError;
				if (!Entry->is_regular_file(EntryError) || !FAssetManager::IsAssetFile(Entry->path()))
				{
					continue;
				}
				if (std::optional<std::string> File = FAssetManager::GetAssetFileOfPath(Entry->path()))
				{
					Files.push_back(std::move(*File));
				}
			}
		}
		std::ranges::sort(Files);
		return FCommandResult::Success(MakeStringList(Files));
	}

	FCommandResult DescribeAsset(FEditor& Editor, const FCommandArguments& Arguments)
	{
		const std::string& File = Arguments.GetString("File");
		if (Editor.GetProject() == nullptr && !FAssetManager::IsBuiltInFile(File))
		{
			return FCommandResult::Failure(std::string(NoProjectError));
		}

		FJsonValue Value;
		std::string Error;
		if (!DescribeAssetFile(File, Value, Error))
		{
			return FCommandResult::Failure(std::format("The file cannot be described: {}.", Error));
		}
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult PlaceAsset(FEditor& Editor, const FCommandArguments& Arguments)
	{
		if (Editor.GetProject() == nullptr)
		{
			return FCommandResult::Failure(std::string(NoProjectError));
		}

		const std::string& File = Arguments.GetString("File");
		std::string Error;
		const std::optional<FAssetPath> Path = FAssetPath::Parse(File, Error);
		if (!Path.has_value())
		{
			return FCommandResult::Failure(std::format("The file cannot be placed: {}.", Error));
		}
		if (!Path->Key.empty() || !FGltfImporter::IsGltfFile(FFileHelper::PathFromUtf8(Path->File)))
		{
			return FCommandResult::Failure(
				std::format("\"{}\" cannot be placed: only a whole model, a .gltf or .glb file, is placed in a scene. "
							"Single assets are used through the properties of components.",
					File));
		}

		WEntity* Parent = nullptr;
		if (Arguments.Has("Parent"))
		{
			Parent = FCommandTargets::FindEntity(Editor, Arguments.GetInteger("Parent"), Error);
			if (Parent == nullptr)
			{
				return FCommandResult::Failure(std::move(Error));
			}
		}

		std::vector<std::string> Warnings;
		WEntity* const Root =
			FGltfImporter::Import(FAssetManager::GetPathOfAssetFile(Path->File), Editor.GetScene(), Error, &Warnings);
		if (Root == nullptr)
		{
			return FCommandResult::Failure(std::format("The file was not placed: {}.", Error));
		}
		if (Parent != nullptr && !Root->SetParent(Parent, ETransformRule::KeepRelative))
		{
			return FCommandResult::Failure("The scene refused to give the model that parent; the log says why.");
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Id", Root->GetId());
		Value.SetMember("Warnings", MakeStringList(Warnings));
		return FCommandResult::Success(std::move(Value));
	}
}

void FAssetCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Asset.Import",
			"Copies a model (.gltf, .glb), a picture (.png, .jpg, .jpeg) or an environment (.hdr) into the content "
			"directory of the project, with the files a .gltf file keeps its data in, and loads it. Returns the "
			"file's name in the project, the path and class of every asset in it, and what was left out.",
			{FCommandParameter::Required(
				 "Source", ECommandArgumentType::String, "The file to import, anywhere on this machine."),
				FCommandParameter::Optional("Folder",
					ECommandArgumentType::String,
					"The folder of the content directory to put it in, for example \"Models\". The content directory "
					"itself if left out.")},
			ECommandEffect::LeavesScene},
		ImportAsset);

	Registry.Register(
		{"Asset.List",
			"Returns the asset files of the project, every model, picture and environment in its content directory, "
			"and the files of assets that the engine has built in, such as \"Engine/Shapes\" with a cube, a sphere "
			"and a plane. By name, in the order of the names. Without a project, the built-in ones alone.",
			{},
			ECommandEffect::LeavesScene},
		ListAssets);

	Registry.Register(
		{"Asset.Describe",
			"Returns the path and class of every asset in a file of the project or in a built-in file, and what was "
			"left out when the file was loaded. The paths are what the properties of components take, with "
			"Object.SetProperty: \"Engine/Shapes#Cube\" as the Mesh of a WStaticMeshComponent shows a cube.",
			{FCommandParameter::Required("File",
				ECommandArgumentType::String,
				"The file's name as Asset.List returns it, for example \"Models/Crate.glb\" or \"Engine/Shapes\".")},
			ECommandEffect::LeavesScene},
		DescribeAsset);

	Registry.Register(
		{"Asset.Place",
			"Puts a model of the project into the scene: one entity named after the file, with the model's nodes as "
			"its descendants and its meshes and materials on their components. Returns the identifier of that entity "
			"and what was left out.",
			{FCommandParameter::Required("File",
				 ECommandArgumentType::String,
				 "The model's name in the project, for example \"Models/Crate.glb\"."),
				FCommandParameter::Optional("Parent",
					ECommandArgumentType::Integer,
					"The identifier of the entity to attach the model to. At the root of the scene if left out.")}},
		PlaceAsset);
}
