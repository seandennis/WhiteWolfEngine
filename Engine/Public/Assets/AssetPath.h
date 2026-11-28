#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <string_view>

/**
 * The name of an asset: which file of the content directory it is in and, for a file that holds several assets,
 * which of them it is. Scene files refer to meshes, materials, textures and environments by this name.
 *
 * As text it is the file, relative to the content directory and with forward slashes, followed by "#" and the key
 * if there is one:
 *
 *     Textures/Wood.png                        the picture as a colour texture
 *     Textures/WoodNormals.png#Linear          the same kind of file as a data texture
 *     Skies/Evening.hdr                        an environment map
 *     Materials/Brick.wwmaterial               a material that is a file of its own
 *     Models/Crate.glb#Meshes/0/Primitives/0   the first part of the first mesh of a glTF file
 *     Models/Crate.glb#Materials/2             its third material
 *
 * Names are compared letter for letter: write a file's name as the file system spells it.
 */
struct FAssetPath
{
	/** The file, relative to the content directory, with forward slashes: "Models/Crate.glb". */
	std::string File;

	/** Which asset of the file. Empty for the asset that a file with only one asset holds. */
	std::string Key;

	/**
	 * Reads an asset path from its text.
	 *
	 * The file has to be a relative path of names separated by single forward slashes; it cannot be empty, start
	 * with a slash, name a drive, or have a part that is "." or "..", so that it cannot lead out of the content
	 * directory. A key cannot contain "#".
	 *
	 * @param OutError Receives the reason if Text is not an asset path.
	 */
	ENGINE_API static std::optional<FAssetPath> Parse(std::string_view Text, std::string& OutError);

	/** Returns the path as text: the file, and "#" and the key if there is one. */
	ENGINE_API std::string ToString() const;

	bool operator==(const FAssetPath& Other) const = default;
};
