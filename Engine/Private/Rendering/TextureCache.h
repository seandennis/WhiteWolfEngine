#pragma once

#include "CoreMinimal.h"

#include "Rendering/RenderContext.h"
#include "Rendering/TextureResource.h"
#include "Rendering/TextureWrap.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <cstddef>
#include <memory>
#include <unordered_map>

class WTexture;

/**
 * The pictures of textures on the graphics card, and the samplers that say how a texture is read. Each renderer
 * has one, and gets from it what its shaders bind for a WTexture.
 *
 * A texture's picture is copied to the graphics card when the texture is first asked for, and again when
 * WTexture::SetImage has given it another picture. The copy is released once the texture has been collected.
 */
class FTextureCache
{
public:
	/**
	 * Creates the samplers and the white texture. Called once, before anything else.
	 *
	 * @return False if the graphics card could not create them.
	 */
	bool Initialize(std::shared_ptr<FRenderContext> InContext);

	/**
	 * Returns the picture of a texture on the graphics card, creating it if it is not there or not current. A
	 * texture that is null, destroyed or without a picture gives the white texture. A new picture is written by
	 * the next call of WritePending, which has to come before anything is drawn with it.
	 *
	 * @return Null if the graphics card could not create the texture.
	 */
	nvrhi::ITexture* FindOrCreate(const WTexture* Texture);

	/** Returns what stands in for a missing texture: one white pixel, which changes nothing it multiplies. */
	nvrhi::ITexture* GetWhiteTexture() const;

	/** Returns the sampler for a pair of wrap modes. It reads smoothly between pixels and between sizes. */
	nvrhi::ISampler* GetSampler(ETextureWrap WrapU, ETextureWrap WrapV) const;

	/**
	 * Writes the picture, with its smaller versions, of every texture that was created since the last call.
	 *
	 * @return How many textures were written.
	 */
	int32 WritePending(nvrhi::ICommandList& CommandList);

	/** Releases the copies of textures that have been collected. */
	void ReleaseUnused();

	/** Returns how many textures have a copy on the graphics card. */
	int32 GetCount() const;

private:
	/** Keeps the graphics card alive for as long as this cache exists. */
	std::shared_ptr<FRenderContext> Context;

	/** One sampler for every pair of wrap modes; the index is WrapU * WrapModeCount + WrapV. */
	static constexpr std::size_t WrapModeCount = 3;
	std::array<nvrhi::SamplerHandle, WrapModeCount * WrapModeCount> Samplers;

	nvrhi::TextureHandle WhiteTexture;

	/**
	 * The copies. The key is only an address: an entry is used only while its image version is that of the
	 * texture at that address.
	 */
	std::unordered_map<const WTexture*, FTextureResource> Textures;
};
