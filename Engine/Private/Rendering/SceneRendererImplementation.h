#pragma once

#include "CoreMinimal.h"

#include "Rendering/RenderContext.h"
#include "Rendering/SceneRenderStatistics.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/StaticMeshBuffers.h"

#include <nvrhi/nvrhi.h>

#include <memory>
#include <unordered_map>

class WStaticMesh;

/** Everything a scene renderer holds. It lives here so that no NVRHI type appears in an engine header. */
struct FSceneRenderer::FImplementation
{
	/** Keeps the graphics card alive for as long as this renderer exists. */
	std::shared_ptr<FRenderContext> Context;

	/** How the per-draw constants reach the shaders, and the set that binds them. */
	nvrhi::BindingLayoutHandle BindingLayout;
	nvrhi::BindingSetHandle BindingSet;

	/** Draws a static mesh without lighting. */
	nvrhi::GraphicsPipelineHandle UnlitPipeline;

	/**
	 * The geometry on the graphics card of every mesh that has been drawn. The key is only an address: an entry
	 * is used only while its weak reference still gives that same mesh.
	 */
	std::unordered_map<const WStaticMesh*, FStaticMeshBuffers> MeshBuffers;

	FSceneRenderStatistics Statistics;
};
