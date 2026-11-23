#pragma once

#include "CoreMinimal.h"

#include <string>

/** An asset as the user interface offers it to choose: what it is called, and what kind of thing it is. */
struct FUiAsset
{
	/** The asset path: what a property that refers to the asset is given, such as "Models/Crate.glb#Materials/0". */
	std::string Path;

	/** The name of the asset's class, such as "WMaterial". */
	std::string ClassName;
};
