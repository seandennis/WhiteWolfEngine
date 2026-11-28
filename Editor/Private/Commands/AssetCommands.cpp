#include "Commands/AssetCommands.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPackage.h"
#include "Assets/AssetPath.h"
#include "Assets/AssetReferenceContext.h"
#include "Assets/EditedAssets.h"
#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Commands/SavedProperties.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/ObjectProperties.h"
#include "Rendering/GltfImporter.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialSerializer.h"
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
		// A file that is one asset, a picture, an environment or a material, is read now, to see that it can be.
		if (FAssetManager::IsOneAsset(File) && FAssetManager::Load(File, OutError) == nullptr)
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

	/**
	 * Returns why the file of a material cannot be put at a path, if a material of that path is still in use although
	 * its file is gone: loading the path gives that material, not the new file. A path that differs from that one
	 * only in its capitals is refused too, being the same file where the file system does not tell capitals apart.
	 *
	 * @return The reason, or nothing if no such material is in use.
	 */
	std::optional<std::string> FindMaterialInUseWithoutFile(const std::string& File)
	{
		const WAsset* const InUse = FAssetManager::FindLoadedIgnoringCase(File);
		if (InUse == nullptr)
		{
			return std::nullopt;
		}
		// A material of another name whose file is there is another file, where capitals are told apart.
		const std::string& InUsePath = InUse->GetAssetPath();
		std::error_code ErrorCode;
		if (std::filesystem::exists(FAssetManager::GetPathOfAssetFile(InUsePath), ErrorCode))
		{
			return std::nullopt;
		}
		if (InUsePath == File)
		{
			return std::format("\"{}\" is still in use although its file is gone", File);
		}
		return std::format(
			"\"{}\" differs only in capitals from \"{}\", which is still in use although its file is gone",
			File,
			InUsePath);
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
				"a .png, .jpg or .jpeg file, an environment a .hdr file, and a material a .wwmaterial file.",
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
		// The editor keeps and writes material files: a new one where one is still in use would be written over.
		if (FMaterialSerializer::IsMaterialFile(Source))
		{
			if (const std::optional<std::string> InUse = FindMaterialInUseWithoutFile(Destination->File))
			{
				return FCommandResult::Failure(std::format("The file was not imported: {}.", *InUse));
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

	FCommandResult CreateMaterial(FEditor& Editor, const FCommandArguments& Arguments)
	{
		if (Editor.GetProject() == nullptr)
		{
			return FCommandResult::Failure(std::string(NoProjectError));
		}

		// Everything is checked before anything is written, and nothing of the project is replaced.
		const auto Refuse = [](std::string_view Reason)
		{
			return FCommandResult::Failure(std::format("The material was not created: {}.", Reason));
		};
		const std::string& Text = Arguments.GetString("Path");
		std::string Error;
		const std::optional<FAssetPath> Path = FAssetPath::Parse(Text, Error);
		if (!Path.has_value())
		{
			return Refuse(Error);
		}
		if (!Path->Key.empty())
		{
			return Refuse(
				std::format("\"{}\" names an asset inside a file, and a material is a file of its own", Text));
		}
		if (!FMaterialSerializer::IsMaterialFile(FFileHelper::PathFromUtf8(Path->File)))
		{
			return Refuse(std::format("the name of a material file ends in \"{}\", as in \"Materials/Brick{}\", "
									  "and \"{}\" does not",
				FMaterialSerializer::FileExtension,
				FMaterialSerializer::FileExtension,
				Text));
		}
		// A folder that the project spells otherwise is the same folder where capitals are not told apart, and a
		// material file is loaded only under the name that its directory gives it: the path would not load.
		if (!FAssetManager::HasFileSystemSpelling(Path->File, Error))
		{
			return Refuse(Error);
		}
		const std::filesystem::path FilePath = FAssetManager::GetPathOfAssetFile(Path->File);
		std::error_code ErrorCode;
		if (std::filesystem::exists(FilePath, ErrorCode))
		{
			return Refuse(std::format("the project already has a file \"{}\"", Path->File));
		}
		if (const std::optional<std::string> InUse = FindMaterialInUseWithoutFile(Path->File))
		{
			return Refuse(*InUse);
		}

		// The file of a new material says what its class gives a new one.
		const WMaterial* const Material = NewObject<WMaterial>();
		if (!FMaterialSerializer::SaveToFile(*Material, FilePath, Error))
		{
			return Refuse(Error);
		}

		FJsonValue Value;
		if (!DescribeAssetFile(Path->File, Value, Error))
		{
			// A file that cannot be loaded is no material, and is not left in the project.
			RemoveFiles({FilePath});
			return Refuse(Error);
		}
		return FCommandResult::Success(std::move(Value));
	}

	/**
	 * Finds the asset of an asset path for a command, loading its file if it is not loaded.
	 *
	 * @param OutError Receives the command's failure, a whole sentence, if there is no such asset.
	 * @return The asset, or null.
	 */
	WAsset* FindAsset(const FEditor& Editor, const std::string& AssetPath, std::string& OutError)
	{
		std::string Error;
		const std::optional<FAssetPath> Path = FAssetPath::Parse(AssetPath, Error);
		if (Path.has_value() && Editor.GetProject() == nullptr && !FAssetManager::IsBuiltInFile(Path->File))
		{
			OutError = NoProjectError;
			return nullptr;
		}
		WAsset* const Asset = FAssetManager::Load(AssetPath, Error);
		if (Asset == nullptr)
		{
			OutError = std::format("The asset could not be loaded: {}.", Error);
		}
		return Asset;
	}

	FCommandResult GetAssetProperties(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		const WAsset* const Asset = FindAsset(Editor, Arguments.GetString("Asset"), Error);
		if (Asset == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		// Every asset that the asset manager gives out is an asset of a package.
		const WAssetPackage* const Package = Asset->GetPackage();
		WW_CHECKF(Package != nullptr, "The asset '{}' has no package", Asset->GetAssetPath());

		FAssetReferenceContext Context;
		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Path", Asset->GetAssetPath());
		Value.SetMember("Class", Asset->GetClass()->GetName());
		Value.SetMember("Unsaved", Editor.GetEditedAssets().HasUnsavedChanges(Asset->GetAssetPath()));
		Value.SetMember("Properties", FObjectProperties::Export(*Asset, Context));
		Value.SetMember("Warnings", MakeStringList(Package->GetWarnings()));
		return FCommandResult::Success(std::move(Value));
	}

	/**
	 * Finds an asset that the editor can change and save for a command: a material that is a file of its own. Goes by
	 * the name first, so that nothing is loaded only to be refused.
	 *
	 * @param OutError Receives the command's failure, a whole sentence, if there is no such asset.
	 * @return The asset, or null.
	 */
	WAsset* FindChangeableAsset(const FEditor& Editor, const std::string& AssetPath, std::string& OutError)
	{
		std::string Error;
		const std::optional<FAssetPath> Path = FAssetPath::Parse(AssetPath, Error);
		if (!Path.has_value())
		{
			OutError = std::format("The asset could not be loaded: {}.", Error);
			return nullptr;
		}
		if (!FEditedAssets::CanChange(*Path))
		{
			OutError =
				std::format("\"{}\" cannot be changed: it is what its file says. Only a material that is a file of its "
							"own, whose name ends in \"{}\", is changed and saved by the editor.",
					AssetPath,
					FMaterialSerializer::FileExtension);
			return nullptr;
		}
		// A file whose name says that it is a material file holds a material and nothing else.
		return FindAsset(Editor, AssetPath, OutError);
	}

	FCommandResult SetAssetProperty(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WAsset* const Asset = FindChangeableAsset(Editor, Arguments.GetString("Asset"), Error);
		if (Asset == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}
		const std::string& PropertyName = Arguments.GetString("Property");
		const FProperty* const Property = FSavedProperties::Find(*Asset, PropertyName, Error);
		if (Property == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		// Kept by the editor and changed in memory: the file is written by Asset.Save.
		if (!Editor.GetEditedAssets().SetProperty(*Asset, *Property, Arguments.GetJson("Value"), Error))
		{
			return FCommandResult::Failure(std::format("The property \"{}\" of the class {} was not changed: {}.",
				PropertyName,
				Asset->GetClass()->GetName(),
				Error));
		}
		return FCommandResult::Success(FJsonValue());
	}

	FCommandResult SaveAsset(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::string Error;
		WAsset* const Asset = FindChangeableAsset(Editor, Arguments.GetString("Asset"), Error);
		if (Asset == nullptr)
		{
			return FCommandResult::Failure(std::move(Error));
		}

		bool bWritten = false;
		if (!Editor.GetEditedAssets().Save(*Asset, bWritten, Error))
		{
			return FCommandResult::Failure(
				std::format("The material \"{}\" was not saved: {}.", Asset->GetAssetPath(), Error));
		}
		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Path", Asset->GetAssetPath());
		Value.SetMember("Written", bWritten);
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
			"Copies a model (.gltf, .glb), a picture (.png, .jpg, .jpeg), an environment (.hdr) or a material "
			"(.wwmaterial) into the content directory of the project, with the files a .gltf file keeps its data "
			"in, and loads it. Returns the file's name in the project, the path and class of every asset in it, "
			"and what was left out, such as a texture of a material that the project does not have.",
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
			"Returns the asset files of the project, every model, picture, environment and material in its content "
			"directory, and the files of assets that the engine has built in, such as \"Engine/Shapes\" with a "
			"cube, a sphere and a plane. By name, in the order of the names. Without a project, the built-in ones "
			"alone.",
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
		{"Asset.GetProperties",
			"Returns what an asset is: its path, its class, whether the editor has changes of it that are not saved "
			"yet (\"Unsaved\"), the values of its saved properties, and what was left out when its file was loaded. "
			"A texture is given as its asset path, and no texture as null. Any asset can be read: a material that is "
			"a file of its own, a material of a model, and without a project the shapes that the engine has built "
			"in.",
			{FCommandParameter::Required("Asset",
				ECommandArgumentType::String,
				"The asset's path, as Asset.Describe returns it, for example \"Materials/Brick.wwmaterial\" or "
				"\"Models/Crate.glb#Materials/0\".")},
			ECommandEffect::LeavesScene},
		GetAssetProperties);

	Registry.Register(
		{"Asset.CreateMaterial",
			"Makes a material in the project: a file of its own in the content directory, with the values of a new "
			"material (white, not a metal, roughness 0.5, no textures). Returns the file's name, which is the "
			"material's asset path, as Asset.Import returns it. That path is what the Material of a "
			"WStaticMeshComponent takes, with Object.SetProperty, and Asset.SetProperty changes its values. Nothing "
			"of the project is replaced, and Edit.Undo does not take the file away.",
			{FCommandParameter::Required("Path",
				ECommandArgumentType::String,
				"Where in the content directory the material's file is to be: a name that ends in \".wwmaterial\", "
				"for example \"Materials/Brick.wwmaterial\". Folders that are not there are made.")},
			ECommandEffect::LeavesScene},
		CreateMaterial);

	Registry.Register(
		{"Asset.SetProperty",
			"Changes the value of one property of a material that is a file of its own, as Asset.CreateMaterial "
			"makes it. Everything that uses the material shows the change at once. The change is kept in the editor "
			"until Asset.Save writes it to the material's file, and Edit.Undo takes it back.",
			{FCommandParameter::Required("Asset",
				 ECommandArgumentType::String,
				 "The asset path of a material that is a file of its own, for example \"Materials/Brick.wwmaterial\"."),
				FCommandParameter::Required("Property",
					ECommandArgumentType::String,
					"The name of the property, as Asset.GetProperties lists it."),
				FCommandParameter::Required("Value",
					ECommandArgumentType::Json,
					"The new value, in the form Asset.GetProperties returns it: a colour is [R, G, B, A] in linear "
					"light, "
					"a texture is the asset path of a picture, with \"#Linear\" for a data texture such as a normal "
					"map, and null is no texture.")},
			ECommandEffect::ChangesAssets},
		SetAssetProperty);

	Registry.Register(
		{"Asset.Save",
			"Writes a material to its file, if the file does not say what the material is: it was changed with "
			"Asset.SetProperty since it was last written, or its file is gone. Returns the material's path and "
			"whether the file was written (\"Written\"). It is no step of the history.",
			{FCommandParameter::Required(
				"Asset", ECommandArgumentType::String, "The asset path of a material that is a file of its own.")},
			ECommandEffect::LeavesScene},
		SaveAsset);

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
