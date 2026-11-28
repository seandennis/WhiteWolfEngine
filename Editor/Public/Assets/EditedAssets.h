#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class FJsonValue;
class FProperty;
class WAsset;
struct FAssetPath;

/**
 * The assets that the editor changes, and keeps until their changes are saved or can no longer be undone.
 *
 * The editor changes only assets that it can also write: material files (FMaterialSerializer), named by their asset
 * paths. A change is made in memory, to the object that the asset manager loaded and that every component which
 * refers to the asset shares, so that it is seen at once wherever the asset is used. The file is written only when
 * the asset is saved (Save).
 *
 * An asset that the editor does not keep is exactly what its file said when it was loaded, so the asset manager may
 * collect it and read it again at any time. An asset that the editor has changed may differ from its file, and the
 * history may change it again, so the editor keeps it in the root set from its first change on: until a history
 * that starts anew finds it saved, or the editor gives it up (another project, the editor's end), which puts it back
 * to what its file says.
 *
 * Changes to assets are steps of the editor's history like changes to its scene: the editor writes down, with the
 * scene, the assets that differ from what they were when the history began, and puts them back on undo and redo.
 * SetProperty is the one way to change an asset. A change made to a kept asset without it is seen as a change made
 * to the scene without a command is; a change made to an asset that the editor does not keep is in no state of the
 * history, and is lost when the asset is collected.
 *
 * Main thread only.
 */
class FEditedAssets
{
public:
	/** Creates a set that keeps no asset. */
	EDITOR_API FEditedAssets();

	/** Gives up the assets that are kept, as ReleaseAll does. */
	EDITOR_API ~FEditedAssets();

	FEditedAssets(const FEditedAssets&) = delete;
	FEditedAssets& operator=(const FEditedAssets&) = delete;

	/**
	 * Returns true if the editor can change and save the asset of an asset path: a material file, which is one asset
	 * under no key. Goes by the name alone.
	 */
	EDITOR_API static bool CanChange(const FAssetPath& Path);

	/** Returns true if the editor can change and save an asset: a material whose asset path is a material file. */
	EDITOR_API static bool CanChange(const WAsset& Asset);

	/**
	 * Gives a saved property of an asset a value, as Object.SetProperty gives one to a component, and keeps the asset
	 * from then on. References are asset paths. A value that is refused leaves the asset as it was; an asset that
	 * was not kept before and is as it was afterwards is not kept.
	 *
	 * @param Asset An asset that the editor can change (CanChange), and the object that the editor keeps for its path
	 * if it keeps one. Anything else is a fatal error.
	 * @param Property A saved property of the asset's class. Anything else is a fatal error.
	 * @param Value The value as JSON, as the property writes it.
	 * @param OutError Receives why the value was refused.
	 * @return True if the property has the value.
	 */
	EDITOR_API bool SetProperty(
		WAsset& Asset, const FProperty& Property, const FJsonValue& Value, std::string& OutError);

	/**
	 * Writes an asset to its file if it has changes that are not saved, or if its file is not there. An asset that
	 * the editor does not keep is what its file says, and is written only if the file is gone.
	 *
	 * @param Asset An asset that the editor can change (CanChange), and the object that the editor keeps for its path
	 * if it keeps one. Anything else is a fatal error.
	 * @param bOutWritten Receives whether the file was written.
	 * @param OutError Receives the reason if the file could not be written; the asset then stays as unsaved as it was.
	 * @return False if the file had to be written and could not be.
	 */
	EDITOR_API bool Save(WAsset& Asset, bool& bOutWritten, std::string& OutError);

	/** Returns true if the editor keeps the asset with this path. */
	EDITOR_API bool IsKept(std::string_view AssetPath) const;

	/** Returns true if the editor keeps the asset with this path, and it differs from what its file last said. */
	EDITOR_API bool HasUnsavedChanges(std::string_view AssetPath) const;

	/** Returns the paths of the assets that have changes that are not saved, in the order of the paths. */
	EDITOR_API std::vector<std::string> GetUnsaved() const;

private:
	friend class FEditor;

	/** An asset that the editor keeps. */
	struct FKeptAsset
	{
		/** The asset, which the editor has put into the root set. */
		TWeakObjectPtr<WAsset> Asset;

		/** What the asset's properties were when the history first had it: what a state that does not name it means. */
		std::string Baseline;

		/** What the asset's properties were when it last agreed with its file. */
		std::string Saved;
	};

	/**
	 * Returns the assets that differ from what they were when the history first had them, as one line of compact
	 * JSON: an object with a member for each, its path, whose value is its properties. Empty if there are none.
	 */
	std::string WriteState() const;

	/**
	 * Puts every kept asset back as a state that WriteState wrote describes it: as the state's member of its path,
	 * or as it was when the history first had it if the state does not name it. An asset that is already so is not
	 * touched. A value that can no longer be had, such as a texture whose file is gone, is a warning in the log, and
	 * its property keeps the value it has.
	 */
	void RestoreState(std::string_view State);

	/**
	 * Begins anew with a history that has no steps: an asset whose changes are saved is no longer kept, and one with
	 * unsaved changes is kept as it is now, which a state that does not name it now means.
	 */
	void BeginHistory();

	/**
	 * Gives every kept asset up: one with unsaved changes is put back to what its file last said, with one warning
	 * in the log that names them all, and none is kept any longer. Nothing is written.
	 */
	void ReleaseAll();

	/** Returns the asset of an entry. One that something other than the editor destroyed is a fatal error. */
	static WAsset& GetAsset(const std::string& AssetPath, const FKeptAsset& Kept);

	/** The assets that the editor keeps, by asset path. */
	std::map<std::string, FKeptAsset, std::less<>> KeptAssets;
};
