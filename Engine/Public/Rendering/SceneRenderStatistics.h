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

	/** How many environment maps were copied to the graphics card, because they were new or had changed. */
	int32 UploadedEnvironmentCount = 0;

	/** How many environment maps have a copy on the graphics card. */
	int32 CachedEnvironmentCount = 0;

	/** One if a sky light lit the picture, zero if not. Always zero in the unlit view mode. */
	int32 SkyLightCount = 0;

	/** How many shadow maps were drawn: four for each directional light that cast shadows. */
	int32 ShadowMapCount = 0;

	/** How many directional lights lit the picture. Always zero in the unlit view mode. */
	int32 DirectionalLightCount = 0;

	/** How many point lights lit the picture. Always zero in the unlit view mode. */
	int32 PointLightCount = 0;

	bool operator==(const FSceneRenderStatistics& Other) const = default;
};
