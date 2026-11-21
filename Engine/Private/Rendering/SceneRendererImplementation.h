#pragma once

#include "CoreMinimal.h"

#include "Rendering/AmbientOcclusion.h"
#include "Rendering/EnvironmentCache.h"
#include "Rendering/MaterialBindings.h"
#include "Rendering/RenderContext.h"
#include "Rendering/SceneRenderStatistics.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/ShadowMaps.h"
#include "Rendering/StaticMeshBuffers.h"
#include "Rendering/TextureResource.h"
#include "Rendering/TextureWrap.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <cstddef>
#include <memory>
#include <unordered_map>

class WMaterial;
class WStaticMesh;
class WTexture;

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

	/** The environments of sky lights on the graphics card. */
	FEnvironmentCache Environments;

	/**
	 * Binds ViewBuffer, DrawBuffer and the two textures of the sky light's environment. Made again whenever
	 * DrawBuffer is replaced by a larger one or the environment is another than the one bound, which the two
	 * pointers remember.
	 */
	nvrhi::BindingSetHandle SceneBindingSet;
	nvrhi::ITexture* BoundReflections = nullptr;
	nvrhi::ITexture* BoundPicture = nullptr;
	nvrhi::ITexture* BoundShadowMaps = nullptr;

	/** The shadow maps of the directional lights. SceneBindingSet binds them too. */
	FShadowMaps Shadows;

	/** Draws the sky light's environment behind the scene. */
	nvrhi::GraphicsPipelineHandle SkyPipeline;

	/** Draws the depth of the scene and the way its surfaces face, ahead of the lit pass, for ambient occlusion. */
	nvrhi::GraphicsPipelineHandle PrepassPipeline;

	/** The ambient occlusion passes. */
	FAmbientOcclusion Occlusion;

	/** How what belongs to a render target reaches the lit pass: its ambient occlusion, and a sampler. */
	nvrhi::BindingLayoutHandle TargetBindingLayout;

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

	/** How the textures of a material reach the shaders: four textures and a sampler for each. */
	nvrhi::BindingLayoutHandle MaterialBindingLayout;

	/** One sampler for every pair of wrap modes; the index is WrapU * WrapModeCount + WrapV. */
	static constexpr std::size_t WrapModeCount = 3;
	std::array<nvrhi::SamplerHandle, WrapModeCount * WrapModeCount> Samplers;

	/** What a material slot without a texture reads: one white pixel, which changes nothing it multiplies. */
	nvrhi::TextureHandle WhiteTexture;

	/** The pictures on the graphics card of every texture that a drawn material has used. Keyed like MeshBuffers. */
	std::unordered_map<const WTexture*, FTextureResource> Textures;

	/**
	 * The binding sets of every material that has been drawn. The key is only an address; null is the key of the
	 * set for components without a material.
	 */
	std::unordered_map<const WMaterial*, FMaterialBindings> Materials;

	/** Creates Samplers and WhiteTexture. Called once, when the renderer is created. Returns false on failure. */
	bool CreateSamplersAndWhiteTexture();

	/**
	 * Returns the picture of a texture on the graphics card, creating it if it is not there or not current. A
	 * texture that is null, destroyed or without a picture gives the white texture.
	 *
	 * @return Null if the graphics card could not create the texture.
	 */
	nvrhi::ITexture* FindOrCreateTexture(WTexture* Texture);

	/** Returns the sampler for a pair of wrap modes. */
	nvrhi::ISampler* GetSampler(ETextureWrap WrapU, ETextureWrap WrapV) const;

	/**
	 * Returns the binding set with the textures of a material, creating it if it does not exist or the material's
	 * textures have changed since it was made.
	 *
	 * @param Material The material, or null for the set of components without one.
	 * @return Null if the graphics card could not create a texture or the set.
	 */
	nvrhi::IBindingSet* FindOrCreateMaterialBindings(WMaterial* Material);

	/**
	 * Writes the picture, with its smaller versions, of every texture that was created since the last call.
	 *
	 * @return How many textures were written.
	 */
	int32 WritePendingTextures(nvrhi::ICommandList& CommandList);

	/** True once a warning about a scene with too many lights has been logged, so that it is logged only once. */
	bool bWarnedAboutLightLimit = false;

	FSceneRenderStatistics Statistics;
};
