#pragma once

#include "CoreMinimal.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class FJsonValue;
class WMaterial;

/**
 * Saves a material as a file of its own and loads one: a file whose name ends in ".wwmaterial", anywhere in a
 * content directory, which is how a material is made and kept that does not come with a model.
 *
 * A material file is readable text that compares cleanly between versions:
 *
 *     {
 *       "Format": "WhiteWolfMaterial",
 *       "Version": 1,
 *       "Properties": {
 *         "BaseColor": [1.0, 0.0, 0.0, 1.0],
 *         "Metallic": 1.0,
 *         "Roughness": 0.3,
 *         "EmissiveColor": [0.0, 0.0, 0.0, 1.0],
 *         "BaseColorTexture": "Textures/Scratches.png",
 *         "MetallicRoughnessTexture": null,
 *         "NormalTexture": "Textures/ScratchesNormals.png#Linear",
 *         ...
 *       }
 *     }
 *
 * "Properties" holds every WPROPERTY() of the material that is not Transient, written by reflection
 * (FObjectProperties), with the values that are the defaults too: a file says everything about its material, and
 * does not change its meaning when a default does. A colour is four numbers in linear light. A texture is written
 * as its asset path (FAssetPath), "Textures/Wood.png#Linear" for a picture read as data, or null for none; one that
 * has no asset path, as a texture made in code, cannot be written, and is written as null with a warning in the
 * log. The file names no other object: neither its own path, which is where the file is, nor an entity or a
 * component.
 *
 * Loading separates what makes a file unusable from what does not, as loading a scene does. A file that is not a
 * material, of a newer version or whose "Properties" is not an object is refused, and the material that it was to
 * be loaded into is left exactly as it was. A problem with a single value (an unknown property, a value of the
 * wrong type, a texture that cannot be loaded) is a warning: that property keeps the value it has, and the others
 * are read. A property that the file does not name keeps its value too.
 */
class FMaterialSerializer
{
public:
	/** The format version this build writes, and the newest one it reads. */
	static constexpr int32 FormatVersion = 1;

	/** The end of the name of a material file, as in "Materials/Brick.wwmaterial". */
	static constexpr std::string_view FileExtension = ".wwmaterial";

	/** Returns true if the name of a file ends in FileExtension, in any case of letters. */
	ENGINE_API static bool IsMaterialFile(const std::filesystem::path& Path);

	/** Returns the material as a JSON document. */
	ENGINE_API static FJsonValue SaveToJson(const WMaterial& Material);

	/** Returns the material as the text of a material file: formatted JSON that ends with a newline. */
	ENGINE_API static std::string SaveToText(const WMaterial& Material);

	/**
	 * Writes the material to a file, making the directories it is in and replacing a file that is there in one step.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return True if the file was written.
	 */
	ENGINE_API static bool SaveToFile(
		const WMaterial& Material, const std::filesystem::path& Path, std::string& OutError);

	/**
	 * Gives a material the values of a material document. The whole document is checked before the material is
	 * changed, so a document that is refused leaves the material as it was. Nothing is logged: whoever knows where
	 * the document comes from says what was wrong with it.
	 *
	 * @param OutError Receives the reason if the document is refused.
	 * @param OutWarnings A sentence is added for each problem that did not stop the load.
	 * @return True if the document was loaded, with or without warnings.
	 */
	ENGINE_API static bool LoadFromJson(
		const FJsonValue& Json, WMaterial& Material, std::string& OutError, std::vector<std::string>& OutWarnings);

	/** Gives a material the values of the text of a material file. See LoadFromJson. */
	ENGINE_API static bool LoadFromText(
		std::string_view Text, WMaterial& Material, std::string& OutError, std::vector<std::string>& OutWarnings);

	/**
	 * Gives a material the values of a material file. See LoadFromJson. A reason for refusing the document begins
	 * with the file's path.
	 */
	ENGINE_API static bool LoadFromFile(const std::filesystem::path& Path,
		WMaterial& Material,
		std::string& OutError,
		std::vector<std::string>& OutWarnings);
};
