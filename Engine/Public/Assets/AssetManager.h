#pragma once

#include "CoreMinimal.h"

#include "Assets/Asset.h"
#include "Object/Class.h"

#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class WAssetPackage;

/** Log category of assets. */
extern ENGINE_API FLogCategory LogAssets;

/**
 * Loads assets from the files of the content directory, by asset path (see FAssetPath).
 *
 * A file is loaded once: its assets form a package, and as long as anything refers to one of them, asking for any
 * of them again returns the same objects. When nothing refers to them any more they are collected like any other
 * object, and the next request loads the file again. What Load returns is not referenced by the manager: keep it in
 * a reflected TObjectPtr, as a component's property does, before the next garbage collection.
 *
 * The files that can be loaded:
 *
 *     .gltf, .glb         the meshes, materials and textures of a glTF 2.0 file, under the keys
 *                         "Meshes/<mesh>/Primitives/<part>", "Materials/<material>" and
 *                         "Textures/<texture>/Srgb" or "Textures/<texture>/Linear", numbered as in the file
 *     .png, .jpg, .jpeg   a texture: without a key a colour texture, with the key "Linear" a data texture
 *     .hdr                an environment map
 *
 * Besides those, the engine has assets built in, which are no files and need no content directory. They are
 * named, loaded and kept like the assets of a file, the file being ShapesFile:
 *
 *     Engine/Shapes#Cube     a cube of one metre
 *     Engine/Shapes#Sphere   a sphere of one metre across
 *     Engine/Shapes#Plane    a square of one metre that lies flat and faces up
 *
 * Each is centred on its origin; an entity that shows one gives it its size with its scale. A file of the content
 * directory cannot take the place of a built-in one: the name means the engine's.
 *
 * All functions are for the main thread.
 */
class FAssetManager
{
public:
	/** The file that the built-in shapes are the assets of, and the asset paths of the shapes. */
	static constexpr std::string_view ShapesFile = "Engine/Shapes";
	static constexpr std::string_view CubeShape = "Engine/Shapes#Cube";
	static constexpr std::string_view SphereShape = "Engine/Shapes#Sphere";
	static constexpr std::string_view PlaneShape = "Engine/Shapes#Plane";

	/**
	 * Says which directory asset paths are relative to, and forgets which of its files are loaded: assets that are
	 * still referred to stay as they are, but asking for them again loads them anew. An empty path means there is
	 * no content directory, and nothing but the built-in assets can be loaded.
	 */
	ENGINE_API static void SetContentDirectory(std::filesystem::path Directory);

	/** Returns the content directory. Empty if there is none. */
	ENGINE_API static const std::filesystem::path& GetContentDirectory();

	/**
	 * Returns the asset with the given path, loading its file if it is not loaded.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return The asset, or null if the text is not an asset path, there is no content directory, the file does
	 * not exist, is of a kind that cannot be loaded or cannot be read, or has no asset with the path's key.
	 */
	ENGINE_API static WAsset* Load(std::string_view AssetPath, std::string& OutError);

	/** Returns the asset with the given path as an AssetType, or null, with the reason, if it is of another class. */
	template <typename AssetType>
	static AssetType* Load(std::string_view AssetPath, std::string& OutError)
	{
		WAsset* const Asset = Load(AssetPath, OutError);
		if (Asset != nullptr && !Asset->IsA(AssetType::StaticClass()))
		{
			OutError = std::format("the asset \"{}\" is a {}, not a {}",
				AssetPath,
				Asset->GetClass()->GetName(),
				AssetType::StaticClass()->GetName());
			return nullptr;
		}
		return static_cast<AssetType*>(Asset);
	}

	/**
	 * Returns the package of a file, with every asset the file has, loading the file if it is not loaded. A
	 * picture's package holds the textures that have been asked for so far.
	 *
	 * @param File The file, relative to the content directory: an asset path without a key.
	 * @param OutError Receives the reason on failure.
	 */
	ENGINE_API static WAssetPackage* LoadPackage(std::string_view File, std::string& OutError);

	/** Returns true if a file with this name is of a kind that can be loaded, going by the end of the name. */
	ENGINE_API static bool IsAssetFile(const std::filesystem::path& Path);

	/** Returns true if the name is that of a file of assets that the engine has built in, such as ShapesFile. */
	ENGINE_API static bool IsBuiltInFile(std::string_view File);

	/** Returns the names of the files of assets that the engine has built in, in the order of the names. */
	ENGINE_API static std::vector<std::string> GetBuiltInFiles();

	/**
	 * Returns the asset path of a file: its path relative to the content directory, with forward slashes. Nothing
	 * if there is no content directory or the file is not inside it. The file need not exist.
	 */
	ENGINE_API static std::optional<std::string> GetAssetFileOfPath(const std::filesystem::path& Path);

	/** Returns where in the file system the file of an asset path is. File must be a valid file of an asset path. */
	ENGINE_API static std::filesystem::path GetPathOfAssetFile(std::string_view File);

private:
	/** Returns the package of a file whose name has been checked, loading the file if it is not loaded. */
	static WAssetPackage* FindOrLoadPackage(const std::string& File, std::string& OutError);

	/** Makes a picture's file a texture, as the key says, and adds it to the picture's package. */
	static WAsset* LoadTexture(WAssetPackage& Package, const std::string& Key, std::string& OutError);
};
