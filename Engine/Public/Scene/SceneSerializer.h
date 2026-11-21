#pragma once

#include "CoreMinimal.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class FJsonValue;
class WScene;

/**
 * Saves scenes as JSON and loads them again.
 *
 * A scene file is readable text that compares cleanly between versions:
 *
 *     {
 *       "Format": "WhiteWolfScene",
 *       "Version": 1,
 *       "NextId": 4,
 *       "Properties": { "Name": "Level 1" },
 *       "Entities": [
 *         {
 *           "Id": 1,
 *           "Properties": { "Name": "Player", "RelativeTransform": { "Location": [0.0, 0.0, 0.0], ... } },
 *           "Components": [
 *             { "Id": 2, "Class": "WHealthComponent", "Properties": { "Health": 100 } }
 *           ]
 *         },
 *         { "Id": 3, "Parent": 1, "Properties": { "Name": "Camera", ... }, "Components": [] }
 *       ]
 *     }
 *
 * "Properties" holds every WPROPERTY() of the object that is not Transient, written by reflection, so a new
 * component type is saved and loaded without any code of its own. Entities appear parents before children, and
 * siblings in order. A reference to an entity or component of the same scene is written as that object's "Id";
 * a reference to anything else cannot be saved and is written as null, with a warning in the log.
 *
 * Loading separates what makes a file unusable from what does not. A file whose structure is wrong (not a scene,
 * a newer format version, a duplicate or missing "Id", a parent that does not exist or is its own descendant) is
 * refused as a whole. A problem with a single value (an unknown property, a value of the wrong type, a reference
 * to an object that is not there) is reported as a warning, and that property keeps its default. A component
 * whose class is not available is kept as a WMissingComponent, so that its data survives the next save.
 */
class FSceneSerializer
{
public:
	/** The format version this build writes, and the newest one it reads. */
	static constexpr int32 FormatVersion = 1;

	/** Returns the scene as a JSON document. */
	ENGINE_API static FJsonValue SaveToJson(const WScene& Scene);

	/** Returns the scene as the text of a scene file: formatted JSON that ends with a newline. */
	ENGINE_API static std::string SaveToText(const WScene& Scene);

	/**
	 * Writes the scene to a file, replacing an existing file in one step.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return True if the file was written.
	 */
	ENGINE_API static bool SaveToFile(const WScene& Scene, const std::filesystem::path& Path, std::string& OutError);

	/**
	 * Creates a scene from a JSON document.
	 *
	 * The new scene is not referenced by anything: hold it in a reflected TObjectPtr or add it to the root set
	 * before the next garbage collection. Components are told that they are attached (OnAttached) only after
	 * every property of the scene has been loaded.
	 *
	 * @param OutError Receives the reason if the document cannot be loaded.
	 * @param OutWarnings If not null, receives one message for each problem that did not stop the load. The same
	 * messages are written to the log.
	 * @return The scene, or null if the document cannot be loaded; nothing is left behind in that case.
	 */
	ENGINE_API static WScene* LoadFromJson(
		const FJsonValue& Json, std::string& OutError, std::vector<std::string>* OutWarnings = nullptr);

	/** Creates a scene from the text of a scene file. See LoadFromJson. */
	ENGINE_API static WScene* LoadFromText(
		std::string_view Text, std::string& OutError, std::vector<std::string>* OutWarnings = nullptr);

	/** Creates a scene from a scene file. See LoadFromJson. */
	ENGINE_API static WScene* LoadFromFile(
		const std::filesystem::path& Path, std::string& OutError, std::vector<std::string>* OutWarnings = nullptr);

private:
	class FLoader;
	class FSaver;
};
