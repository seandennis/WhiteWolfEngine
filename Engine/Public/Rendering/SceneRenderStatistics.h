#pragma once

#include "CoreMinimal.h"

/** What FSceneRenderer did the last time it drew a scene. For tests, tools and the editor's statistics. */
struct FSceneRenderStatistics
{
	/** How many meshes were drawn; a mesh shown by two components counts twice. */
	int32 DrawnMeshCount = 0;

	/** How many meshes had their geometry copied to the graphics card, because it was new or had changed. */
	int32 UploadedMeshCount = 0;

	/** How many meshes have their geometry on the graphics card, whether or not they were drawn. */
	int32 CachedMeshCount = 0;

	bool operator==(const FSceneRenderStatistics& Other) const = default;
};
