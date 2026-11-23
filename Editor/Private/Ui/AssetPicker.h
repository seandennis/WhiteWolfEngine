#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <vector>

class FEditor;
class WClass;
struct FUiFrame;

/**
 * The list from which the user chooses an asset for a property that refers to one: the assets of the project, and
 * those that the engine has built in, that the property can be given.
 *
 * A picture and an environment are one asset each, and a line of the list. A model, and a file that the engine has
 * built in, hold several: such a file is a line that opens a list of its own, and is looked into only when that
 * list is opened, because a model may be large. Which files there are is found out when the list is opened, and no
 * file is looked into twice while it stays open.
 *
 * One picker serves every property of a panel: only one list is open at a time.
 */
class FAssetPicker
{
public:
	/** What Dear ImGui knows the list by, among the items around the button that opens it. */
	static constexpr const char* PopupId = "##Assets";

	/**
	 * Lays the list out if it is open, for the property whose button opened it. Call it where the button was
	 * laid out, among the same items.
	 *
	 * @param AssetClass The class that the property refers to. Assets of it and of classes derived from it are
	 * offered.
	 * @param ItemName The name of the property's widget. The lines of the list are named below it:
	 * "<ItemName>/Assets/None", "<ItemName>/Assets/Models/Crate.glb" for a file, and
	 * "<ItemName>/Assets/Models/Crate.glb#Materials/0" for an asset. In the list of a file, the line that says
	 * that it holds nothing for the property is "<ItemName>/Assets/<File>/Nothing", and the line that says why
	 * it could not be looked into "<ItemName>/Assets/<File>/Error".
	 * @return The path of the asset that was chosen in this frame, which is empty if no asset was chosen in the
	 * place of the one there was; nothing if nothing was chosen.
	 */
	std::optional<std::string> Draw(FUiFrame& Frame, const WClass& AssetClass, const std::string& ItemName);

private:
	/** What a file was found to hold when it was looked into. */
	struct FFileAssets
	{
		std::string File;

		/** The paths of its assets that the property can be given. */
		std::vector<std::string> Paths;

		/** Why the file could not be looked into; empty if it could. */
		std::string Error;
	};

	/** Returns what a file holds that the property can be given. The file is looked into the first time only. */
	const FFileAssets& FindAssets(FEditor& Editor, const std::string& File, const WClass& AssetClass);

	/** The files that can hold something for the property, as of when the list was opened. */
	std::vector<std::string> Files;

	/** The files that have been looked into since the list was opened. */
	std::vector<FFileAssets> FilesLookedInto;
};
