#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <vector>

class FEditor;
class WClass;
struct FUiFrame;

/**
 * The button and the list from which the user chooses an asset for a property that refers to one: the assets of
 * the project, and those that the engine has built in, that the property can be given.
 *
 * A picture and an environment are one asset each, and a line of the list. A model, and a file that the engine has
 * built in, hold several: such a file is a line that a click opens, with its assets as the lines below it. The
 * file is looked into when its line is opened and not before, because a model may be large, and a mouse that only
 * passes over a line opens nothing. Which files there are is found out when the list is opened, and no file is
 * looked into twice while it stays open.
 *
 * One picker serves every property of a panel: only one list is open at a time.
 */
class FAssetPicker
{
public:
	/**
	 * Lays out the button that opens the list, where the next item of the row would be, and the list while it is
	 * open, for the property that the button belongs to.
	 *
	 * @param AssetClass The class that the property refers to. Assets of it and of classes derived from it are
	 * offered.
	 * @param ItemName The name of the property's widget. The button is "<ItemName>/Choose", and the lines of the
	 * list are named below the widget: "<ItemName>/Assets/None", "<ItemName>/Assets/Models/Crate.glb" for a
	 * file, and "<ItemName>/Assets/Models/Crate.glb#Materials/0" for an asset. Below an opened file, the line
	 * that says that it holds nothing for the property is "<ItemName>/Assets/<File>/Nothing", and the line that
	 * says why it could not be looked into "<ItemName>/Assets/<File>/Error".
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

	/** Lays out the lines of the list, which is open, and returns what Draw returns. */
	std::optional<std::string> DrawLines(FUiFrame& Frame, const WClass& AssetClass, const std::string& ItemName);

	/** Returns what a file holds that the property can be given. The file is looked into the first time only. */
	const FFileAssets& FindAssets(FEditor& Editor, const std::string& File, const WClass& AssetClass);

	/** The files that can hold something for the property, as of when the list was opened. */
	std::vector<std::string> Files;

	/** The files that have been looked into since the list was opened. */
	std::vector<FFileAssets> FilesLookedInto;
};
