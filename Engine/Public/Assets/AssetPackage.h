#pragma once

#include "CoreMinimal.h"

#include "Assets/Asset.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

#include <string>
#include <string_view>
#include <vector>

/**
 * The assets of one file of the content directory, each under its key.
 *
 * A package and its assets refer to each other, so they are kept or collected together. FAssetManager creates
 * packages; ask it for one with LoadPackage.
 */
WCLASS()
class ENGINE_API WAssetPackage : public WObject
{
	WW_DECLARE_CLASS(WAssetPackage, WObject)

public:
	/** Returns the file the package was loaded from, relative to the content directory: "Models/Crate.glb". */
	const std::string& GetFile() const;

	/** Returns the asset with the given key, or null if the file has none. */
	WAsset* FindAsset(std::string_view Key) const;

	/** Returns the keys of the package's assets, in the order the assets were added. */
	const std::vector<std::string>& GetKeys() const;

	/** Returns the assets, in the same order as GetKeys. */
	const std::vector<TObjectPtr<WAsset>>& GetAssets() const;

	/** Returns what was left out or could not be read when the file was loaded, one line each. */
	const std::vector<std::string>& GetWarnings() const;

private:
	friend class FAssetManager;

	/**
	 * Adds an asset under a key, and gives the asset its path. The key must not be taken, and the asset must not
	 * belong to a package yet.
	 */
	void AddAsset(const std::string& Key, WAsset& Asset);

	std::string File;

	/** The keys, and at the same places the assets. */
	std::vector<std::string> Keys;

	WPROPERTY(Transient)
	std::vector<TObjectPtr<WAsset>> Assets;

	std::vector<std::string> Warnings;
};
