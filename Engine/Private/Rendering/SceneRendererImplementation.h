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

	/** How the data of a picture reaches the shaders that draw meshes: the array of draws and the lights. */
	nvrhi::BindingLayoutHandle SceneBindingLayout;

	/** The lights of the picture being drawn; an FViewConstants. */
	nvrhi::BufferHandle ViewBuffer;

	/** The meshes of the picture being drawn; an array of FDrawData that grows with the largest scene drawn. */
	nvrhi::BufferHandle DrawBuffer;
	int32 DrawBufferCapacity = 0;

	/** Binds ViewBuffer and DrawBuffer. Made again whenever DrawBuffer is replaced by a larger one. */
	nvrhi::BindingSetHandle SceneBindingSet;

	/** Draws meshes lit, into the high dynamic range buffer of a render target. */
	nvrhi::GraphicsPipelineHandle LitPipeline;

	/** Draws meshes in their base colour, straight onto the surface of a render target. */
	nvrhi::GraphicsPipelineHandle UnlitPipeline;

	/** Fits the lit scene to the display. Its binding layout gives it the lit scene and the exposure. */
	nvrhi::BindingLayoutHandle TonemapBindingLayout;
	nvrhi::GraphicsPipelineHandle TonemapPipeline;

	/**
	 * The geometry on the graphics card of every mesh that has been drawn. The key is only an address: an entry
	 * is used only while its geometry version is that of the mesh at that address.
	 */
	std::unordered_map<const WStaticMesh*, FStaticMeshBuffers> MeshBuffers;

	/** True once a warning about a scene with too many lights has been logged, so that it is logged only once. */
	bool bWarnedAboutLightLimit = false;

	FSceneRenderStatistics Statistics;
};
