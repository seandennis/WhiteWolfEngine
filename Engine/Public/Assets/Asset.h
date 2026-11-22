#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

#include <string>

class WAssetPackage;

/**
 * The base class of what a game is made from and scenes refer to: meshes, materials, textures, environments.
 *
 * An asset that FAssetManager loaded from a file of the content directory has an asset path, the name under which
 * it can be loaded again; a scene file writes a reference to the asset as that path. An asset that was made in
 * code, with NewObject, has none, and a reference to it cannot be saved.
 *
 * The assets of one file belong to one WAssetPackage and stay alive together: as long as anything refers to one
 * of them, all are kept, so that a file is never loaded in part twice.
 */
WCLASS()
class ENGINE_API WAsset : public WObject
{
	WW_DECLARE_CLASS(WAsset, WObject)

public:
	/** Returns the asset's path as text, such as "Models/Crate.glb#Materials/0". Empty for an asset made in code. */
	const std::string& GetAssetPath() const;

	/** Returns the package of the file the asset was loaded from. Null for an asset made in code. */
	WAssetPackage* GetPackage() const;

private:
	friend class WAssetPackage;

	std::string AssetPath;

	/** Keeps the other assets of the file alive. Set when the asset is added to its package. */
	WPROPERTY(Transient)
	TObjectPtr<WAssetPackage> Package;
};
