#include "Assets/AssetManager.h"

#include "Assets/AssetPackage.h"
#include "Assets/AssetPath.h"
#include "Core/FileHelper.h"
#include "Math/Vector.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/GltfImporter.h"
#include "Rendering/HdrImage.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialSerializer.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureColorSpace.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

constinit FLogCategory LogAssets("LogAssets", ELogVerbosity::Log);

namespace
{
	/** What the manager remembers between calls. */
	struct FAssetManagerState
	{
		std::filesystem::path ContentDirectory;

		/** The packages that have been loaded, by file. Weak: a package lives as long as its assets are used. */
		std::unordered_map<std::string, TWeakObjectPtr<WAssetPackage>> Packages;

		/**
		 * The files that are being read, the one that was begun last at the end. Reading a material file loads its
		 * textures, which are other files.
		 */
		std::vector<std::string> FilesBeingRead;
	};

	FAssetManagerState& GetState()
	{
		static FAssetManagerState State;
		return State;
	}

	/** The kinds of file that hold assets. */
	enum class EAssetFileKind : uint8
	{
		Unknown,
		Gltf,
		Picture,
		Environment,
		Material,
	};

	/** Marks a file as being read for as long as it lives, however the reading ends. */
	class FFileBeingRead
	{
	public:
		FFileBeingRead(std::vector<std::string>& InFilesBeingRead, const std::string& File)
			: FilesBeingRead(InFilesBeingRead)
		{
			FilesBeingRead.push_back(File);
		}

		~FFileBeingRead()
		{
			FilesBeingRead.pop_back();
		}

		FFileBeingRead(const FFileBeingRead&) = delete;
		FFileBeingRead& operator=(const FFileBeingRead&) = delete;

	private:
		std::vector<std::string>& FilesBeingRead;
	};

	/** Returns the kind of a file, going by the end of its name, whatever the case of its letters. */
	EAssetFileKind GetFileKind(const std::filesystem::path& Path)
	{
		std::string Extension = FFileHelper::PathToUtf8(Path.extension());
		std::ranges::transform(Extension,
			Extension.begin(),
			[](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });

		if (FGltfImporter::IsGltfFile(Path))
		{
			return EAssetFileKind::Gltf;
		}
		if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg")
		{
			return EAssetFileKind::Picture;
		}
		if (Extension == ".hdr")
		{
			return EAssetFileKind::Environment;
		}
		if (FMaterialSerializer::IsMaterialFile(Path))
		{
			return EAssetFileKind::Material;
		}
		return EAssetFileKind::Unknown;
	}

	/**
	 * The size of the built-in shapes, in metres: one, so that the scale of an entity that shows one is the size
	 * of what it shows.
	 */
	constexpr float ShapeSize = 1.0f;

	/** Makes the built-in shapes, each under the key that its asset path gives it. */
	void MakeShapes(std::vector<std::pair<std::string, WAsset*>>& OutAssets)
	{
		const auto AddShape = [&OutAssets](std::string_view Path, WStaticMesh* Mesh)
		{
			std::string Error;
			const std::optional<FAssetPath> AssetPath = FAssetPath::Parse(Path, Error);
			const bool bIsShape =
				AssetPath.has_value() && AssetPath->File == FAssetManager::ShapesFile && Mesh != nullptr;
			WW_CHECKF(bIsShape, "The built-in shape '{}' could not be made", Path);
			OutAssets.emplace_back(AssetPath->Key, Mesh);
		};

		AddShape(FAssetManager::CubeShape, FMeshShapes::CreateBox(FVector3f(ShapeSize)));
		AddShape(FAssetManager::SphereShape, FMeshShapes::CreateSphere(ShapeSize * 0.5f));
		AddShape(FAssetManager::PlaneShape, FMeshShapes::CreatePlane(ShapeSize, ShapeSize));
	}

	/**
	 * Reads the assets of a file of the content directory.
	 *
	 * @param OutAssets Receives the assets that the file holds at once, each with its key. A picture holds none
	 * until a texture is asked for.
	 * @return False, with the reason in OutError, if the file is of no kind that holds assets, is not there or
	 * cannot be read, or is a material file asked for under another spelling than its directory gives it.
	 */
	bool ReadAssetFile(const std::string& File,
		std::vector<std::pair<std::string, WAsset*>>& OutAssets,
		std::vector<std::string>& OutWarnings,
		std::string& OutError)
	{
		const std::filesystem::path Path = FAssetManager::GetPathOfAssetFile(File);
		const EAssetFileKind Kind = GetFileKind(Path);
		if (Kind == EAssetFileKind::Unknown)
		{
			OutError = std::format("\"{}\" is not a kind of file that holds assets", File);
			return false;
		}
		std::error_code ErrorCode;
		if (!std::filesystem::is_regular_file(Path, ErrorCode))
		{
			OutError = std::format("the content directory has no file \"{}\"", File);
			return false;
		}

		if (Kind == EAssetFileKind::Gltf)
		{
			return FGltfImporter::LoadAssets(Path, OutAssets, OutError, &OutWarnings);
		}
		if (Kind == EAssetFileKind::Environment)
		{
			FHdrImage Image;
			if (!FImageFile::LoadHdr(Path, Image, OutError))
			{
				return false;
			}
			WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
			if (!Environment->SetImage(std::move(Image), OutError))
			{
				return false;
			}
			OutAssets.emplace_back(std::string(), Environment);
		}
		if (Kind == EAssetFileKind::Material)
		{
			// One file is one material. Under a second spelling of its name it would be a second object of the same
			// file, which could be given other values and saved over the first.
			if (!FAssetManager::HasFileSystemSpelling(File, OutError))
			{
				return false;
			}

			// A material is made with what its class gives a new one, and then given what the file says.
			WMaterial* const Material = NewObject<WMaterial>();
			std::vector<std::string> Problems;
			if (!FMaterialSerializer::LoadFromFile(Path, *Material, OutError, Problems))
			{
				return false;
			}

			// What a material file could not give its material is said where whoever uses the material looks: a
			// scene whose material lost a texture says so in the log.
			for (std::string& Problem : Problems)
			{
				WW_LOG(LogAssets, Warning, "Loading '{}': {}", File, Problem);
				OutWarnings.push_back(std::move(Problem));
			}
			OutAssets.emplace_back(std::string(), Material);
		}
		return true;
	}
}

void FAssetManager::SetContentDirectory(std::filesystem::path Directory)
{
	FAssetManagerState& State = GetState();
	State.ContentDirectory = std::move(Directory);

	// What the engine has built in is the same whatever the content directory is, and stays loaded.
	std::erase_if(State.Packages,
		[](const std::pair<const std::string, TWeakObjectPtr<WAssetPackage>>& Entry)
		{ return !IsBuiltInFile(Entry.first); });
}

const std::filesystem::path& FAssetManager::GetContentDirectory()
{
	return GetState().ContentDirectory;
}

WAsset* FAssetManager::Load(std::string_view AssetPath, std::string& OutError)
{
	const std::optional<FAssetPath> Path = FAssetPath::Parse(AssetPath, OutError);
	if (!Path.has_value())
	{
		return nullptr;
	}

	WAssetPackage* const Package = FindOrLoadPackage(Path->File, OutError);
	if (Package == nullptr)
	{
		return nullptr;
	}
	if (WAsset* const Asset = Package->FindAsset(Path->Key))
	{
		return Asset;
	}

	// A picture becomes a texture when it is first asked for, as a colour texture or as a data texture: keeping
	// both for every picture would hold each in memory twice.
	if (GetFileKind(FFileHelper::PathFromUtf8(Path->File)) == EAssetFileKind::Picture)
	{
		return LoadTexture(*Package, Path->Key, OutError);
	}

	OutError = std::format("the file \"{}\" has no asset \"{}\"", Path->File, Path->Key);
	return nullptr;
}

WAssetPackage* FAssetManager::LoadPackage(std::string_view File, std::string& OutError)
{
	const std::optional<FAssetPath> Path = FAssetPath::Parse(File, OutError);
	if (!Path.has_value())
	{
		return nullptr;
	}
	if (!Path->Key.empty())
	{
		OutError = std::format("\"{}\" names one asset of a file, where the file is wanted", File);
		return nullptr;
	}
	return FindOrLoadPackage(Path->File, OutError);
}

WAsset* FAssetManager::FindLoaded(std::string_view AssetPath)
{
	std::string Error;
	const std::optional<FAssetPath> Path = FAssetPath::Parse(AssetPath, Error);
	if (!Path.has_value())
	{
		return nullptr;
	}

	const FAssetManagerState& State = GetState();
	const auto Found = State.Packages.find(Path->File);
	if (Found == State.Packages.end())
	{
		return nullptr;
	}
	const WAssetPackage* const Package = Found->second.Get();
	return Package != nullptr ? Package->FindAsset(Path->Key) : nullptr;
}

bool FAssetManager::IsAssetFile(const std::filesystem::path& Path)
{
	return GetFileKind(Path) != EAssetFileKind::Unknown;
}

bool FAssetManager::IsBuiltInFile(std::string_view File)
{
	return File == ShapesFile;
}

std::vector<std::string> FAssetManager::GetBuiltInFiles()
{
	return {std::string(ShapesFile)};
}

bool FAssetManager::CanHoldAssetsOfClass(std::string_view File, const WClass& Class)
{
	// An asset can be given to whatever asks for its own class or for one that its class derives from.
	const auto CanBeGiven = [&Class](const WClass* HeldClass)
	{
		return HeldClass->IsChildOf(&Class);
	};
	if (IsBuiltInFile(File))
	{
		return CanBeGiven(WStaticMesh::StaticClass());
	}

	switch (GetFileKind(FFileHelper::PathFromUtf8(File)))
	{
		case EAssetFileKind::Gltf:
			return CanBeGiven(WStaticMesh::StaticClass()) || CanBeGiven(WMaterial::StaticClass()) ||
				CanBeGiven(WTexture::StaticClass());
		case EAssetFileKind::Picture:
			return CanBeGiven(WTexture::StaticClass());
		case EAssetFileKind::Environment:
			return CanBeGiven(WEnvironmentMap::StaticClass());
		case EAssetFileKind::Material:
			return CanBeGiven(WMaterial::StaticClass());
		case EAssetFileKind::Unknown:
		default:
			return false;
	}
}

bool FAssetManager::IsOneAsset(std::string_view File)
{
	if (IsBuiltInFile(File))
	{
		return false;
	}
	const EAssetFileKind Kind = GetFileKind(FFileHelper::PathFromUtf8(File));
	return Kind == EAssetFileKind::Picture || Kind == EAssetFileKind::Environment || Kind == EAssetFileKind::Material;
}

std::optional<std::string> FAssetManager::GetAssetFileOfPath(const std::filesystem::path& Path)
{
	const std::filesystem::path& ContentDirectory = GetState().ContentDirectory;
	if (ContentDirectory.empty() || Path.empty())
	{
		return std::nullopt;
	}

	// Both are made absolute and free of "." and "..", and of links where they exist, before they are compared.
	std::error_code ErrorCode;
	const std::filesystem::path AbsoluteDirectory = std::filesystem::weakly_canonical(ContentDirectory, ErrorCode);
	if (ErrorCode)
	{
		return std::nullopt;
	}
	const std::filesystem::path AbsolutePath = std::filesystem::weakly_canonical(Path, ErrorCode);
	if (ErrorCode)
	{
		return std::nullopt;
	}

	const std::filesystem::path Relative = AbsolutePath.lexically_relative(AbsoluteDirectory);
	const std::u8string GenericText = Relative.generic_u8string();
	const std::string Text(GenericText.begin(), GenericText.end());

	// A path that leads out of the directory starts with "..", which is not a valid asset path, and neither is
	// the directory itself, ".".
	std::string Error;
	const std::optional<FAssetPath> AssetPath = FAssetPath::Parse(Text, Error);
	if (!AssetPath.has_value() || !AssetPath->Key.empty())
	{
		return std::nullopt;
	}
	return AssetPath->File;
}

std::filesystem::path FAssetManager::GetPathOfAssetFile(std::string_view File)
{
	return GetState().ContentDirectory / FFileHelper::PathFromUtf8(File);
}

bool FAssetManager::HasFileSystemSpelling(std::string_view File, std::string& OutError)
{
	const std::filesystem::path& ContentDirectory = GetState().ContentDirectory;
	if (ContentDirectory.empty())
	{
		OutError = "there is no content directory";
		return false;
	}
	const std::optional<FAssetPath> Path = FAssetPath::Parse(File, OutError);
	if (!Path.has_value())
	{
		return false;
	}
	if (!Path->Key.empty())
	{
		OutError = std::format("\"{}\" names one asset of a file, where the file is wanted", File);
		return false;
	}

	// Each part is looked for among the entries of the folder above it, under the name that the folder gives it.
	const std::string_view FileText = Path->File;
	std::filesystem::path Folder = ContentDirectory;
	std::size_t PartStart = 0;
	while (true)
	{
		const std::size_t Slash = FileText.find('/', PartStart);
		const bool bIsLastPart = Slash == std::string_view::npos;
		const std::size_t PartEnd = bIsLastPart ? FileText.size() : Slash;
		const std::string_view PartText = FileText.substr(PartStart, PartEnd - PartStart);

		// Windows keeps no name that ends in a period or a space: it makes and opens the name without them, which is
		// another name, also where nothing of that name is there yet.
		const bool bEndsInPeriod = PartText.ends_with('.');
		if (bEndsInPeriod || PartText.ends_with(' '))
		{
			OutError = std::format("the name \"{}\" ends in a {}, which Windows does not keep",
				PartText,
				bEndsInPeriod ? "period" : "space");
			return false;
		}

		const std::filesystem::path Part = FFileHelper::PathFromUtf8(PartText);
		const std::filesystem::path PartPath = Folder / Part;

		std::error_code ErrorCode;
		bool bIsListed = false;
		for (std::filesystem::directory_iterator Entry(Folder, ErrorCode);
			 !ErrorCode && Entry != std::filesystem::directory_iterator();
			 Entry.increment(ErrorCode))
		{
			// The names are compared as the file system keeps them, character for character.
			if (Entry->path().filename().native() == Part.native())
			{
				bIsListed = true;
				break;
			}
		}
		if (ErrorCode)
		{
			OutError = std::format("the folder \"{}\" cannot be listed: {}",
				FFileHelper::PathToUtf8(Folder),
				FFileHelper::DescribeError(ErrorCode));
			return false;
		}

		if (!bIsListed)
		{
			// Not under this name: there under another spelling, or not there at all, and then neither is anything
			// below it. A part that cannot even be looked at is taken to be not there: whatever then reads or writes
			// the file fails, with its own reason.
			if (std::filesystem::exists(PartPath, ErrorCode))
			{
				// Called what it is on disk, which is not always what the path would have it be.
				const bool bIsFolder = std::filesystem::is_directory(PartPath, ErrorCode);
				OutError =
					std::format("the content directory has no {} \"{}\", only one whose name is spelt differently",
						bIsFolder ? "folder" : "file",
						FileText.substr(0, PartEnd));
				return false;
			}
			return true;
		}

		// Nothing is below the file itself, nor below a part that is not a folder.
		if (bIsLastPart || !std::filesystem::is_directory(PartPath, ErrorCode))
		{
			return true;
		}
		Folder = PartPath;
		PartStart = Slash + 1;
	}
}

WAssetPackage* FAssetManager::FindOrLoadPackage(const std::string& File, std::string& OutError)
{
	FAssetManagerState& State = GetState();
	const bool bIsBuiltIn = IsBuiltInFile(File);
	if (!bIsBuiltIn && State.ContentDirectory.empty())
	{
		OutError = std::format("the asset file \"{}\" cannot be loaded: there is no content directory", File);
		return nullptr;
	}

	const auto Found = State.Packages.find(File);
	if (Found != State.Packages.end())
	{
		if (WAssetPackage* const Loaded = Found->second.Get())
		{
			return Loaded;
		}
	}

	// A file that refers to itself, directly or through other files, would be read again and again without end.
	// Asked for again while it is being read, it is a reference that cannot be followed, and the file that asked
	// is read without it.
	if (std::ranges::find(State.FilesBeingRead, File) != State.FilesBeingRead.end())
	{
		OutError = std::format("the asset file \"{}\" refers to itself, directly or through another file", File);
		return nullptr;
	}

	std::vector<std::pair<std::string, WAsset*>> Assets;
	std::vector<std::string> Warnings;
	if (bIsBuiltIn)
	{
		MakeShapes(Assets);
	}
	else
	{
		const FFileBeingRead BeingRead(State.FilesBeingRead, File);
		if (!ReadAssetFile(File, Assets, Warnings, OutError))
		{
			return nullptr;
		}
	}

	WAssetPackage* const Package = NewObject<WAssetPackage>();
	Package->File = File;
	Package->Warnings = std::move(Warnings);
	for (const std::pair<std::string, WAsset*>& Asset : Assets)
	{
		Package->AddAsset(Asset.first, *Asset.second);
	}

	// The packages that have been collected since are forgotten, so that the list does not only ever grow.
	std::erase_if(State.Packages,
		[](const std::pair<const std::string, TWeakObjectPtr<WAssetPackage>>& Entry)
		{ return !Entry.second.IsValid(); });
	State.Packages.insert_or_assign(File, TWeakObjectPtr<WAssetPackage>(Package));

	WW_LOG(
		LogAssets, Log, "Loaded '{}': {} assets, {} warnings", File, Package->Assets.size(), Package->Warnings.size());
	return Package;
}

WAsset* FAssetManager::LoadTexture(WAssetPackage& Package, const std::string& Key, std::string& OutError)
{
	if (!Key.empty() && Key != LinearTextureKey)
	{
		OutError = std::format("the picture \"{}\" has no asset \"{}\": a picture is a colour texture without a key, "
							   "or a data texture with the key \"{}\"",
			Package.File,
			Key,
			LinearTextureKey);
		return nullptr;
	}

	FImage Image;
	if (!FImageFile::Load(GetPathOfAssetFile(Package.File), Image, OutError))
	{
		return nullptr;
	}
	WTexture* const Texture = NewObject<WTexture>();
	const ETextureColorSpace ColorSpace = Key.empty() ? ETextureColorSpace::Srgb : ETextureColorSpace::Linear;
	if (!Texture->SetImage(std::move(Image), ColorSpace, OutError))
	{
		return nullptr;
	}

	Package.AddAsset(Key, *Texture);
	return Texture;
}
