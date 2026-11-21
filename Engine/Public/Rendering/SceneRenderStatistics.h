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

	/** How many textures had their picture copied to the graphics card, because it was new or had changed. */
	int32 UploadedTextureCount = 0;

	/** How many textures have their picture on the graphics card, whether or not they were used. */
	int32 CachedTextureCount = 0;

	/** How many directional lights lit the picture. Always zero in the unlit view mode. */
	int32 DirectionalLightCount = 0;

	/** How many point lights lit the picture. Always zero in the unlit view mode. */
	int32 PointLightCount = 0;

	bool operator==(const FSceneRenderStatistics& Other) const = default;
};
