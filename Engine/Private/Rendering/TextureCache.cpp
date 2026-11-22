#include "Rendering/TextureCache.h"

#include "Math/Color.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/Image.h"
#include "Rendering/MipChain.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureColorSpace.h"

#include <algorithm>
#include <utility>
#include <vector>

bool FTextureCache::Initialize(std::shared_ptr<FRenderContext> InContext)
{
	Context = std::move(InContext);
	nvrhi::IDevice& Device = *Context->GetDevice();

	// In the order of ETextureWrap.
	const std::array<nvrhi::SamplerAddressMode, WrapModeCount> AddressModes{
		nvrhi::SamplerAddressMode::Repeat,
		nvrhi::SamplerAddressMode::MirroredRepeat,
		nvrhi::SamplerAddressMode::ClampToEdge,
	};
	for (std::size_t WrapU = 0; WrapU < WrapModeCount; ++WrapU)
	{
		for (std::size_t WrapV = 0; WrapV < WrapModeCount; ++WrapV)
		{
			// Textures are read smoothly: between pixels, and between their smaller versions.
			nvrhi::SamplerDesc Description;
			Description.setAllFilters(true);
			Description.setAddressU(AddressModes[WrapU]);
			Description.setAddressV(AddressModes[WrapV]);

			nvrhi::SamplerHandle& Sampler = Samplers[WrapU * WrapModeCount + WrapV];
			Sampler = Device.createSampler(Description);
			if (Sampler == nullptr)
			{
				return false;
			}
		}
	}

	nvrhi::TextureDesc Description;
	Description.width = 1;
	Description.height = 1;
	Description.format = nvrhi::Format::RGBA8_UNORM;
	Description.debugName = "WhiteTexture";
	Description.initialState = nvrhi::ResourceStates::ShaderResource;
	Description.keepInitialState = true;
	WhiteTexture = Device.createTexture(Description);
	if (WhiteTexture == nullptr)
	{
		return false;
	}

	const FColor WhitePixel(255, 255, 255, 255);
	nvrhi::ICommandList& CommandList = *Context->GetImmediateCommandList();
	CommandList.open();
	CommandList.writeTexture(WhiteTexture, 0, 0, &WhitePixel, sizeof(FColor));
	CommandList.close();
	Device.executeCommandList(&CommandList);
	return true;
}

nvrhi::ITexture* FTextureCache::FindOrCreate(const WTexture* Texture)
{
	if (!IsValid(Texture) || !Texture->HasImage())
	{
		return WhiteTexture;
	}

	const auto Existing = Textures.find(Texture);
	if (Existing != Textures.end() && Existing->second.ImageVersion == Texture->GetImageVersion())
	{
		return Existing->second.Handle;
	}

	// Each smaller version halves the longer side, until it is one pixel.
	const FImage& Image = Texture->GetImage();
	uint32 MipCount = 1;
	for (int32 Side = std::max(Image.GetWidth(), Image.GetHeight()); Side > 1; Side /= 2)
	{
		++MipCount;
	}

	nvrhi::TextureDesc Description;
	Description.width = static_cast<uint32>(Image.GetWidth());
	Description.height = static_cast<uint32>(Image.GetHeight());
	Description.mipLevels = MipCount;
	Description.format =
		Texture->GetColorSpace() == ETextureColorSpace::Srgb ? nvrhi::Format::SRGBA8_UNORM : nvrhi::Format::RGBA8_UNORM;
	Description.debugName = "Texture";
	Description.initialState = nvrhi::ResourceStates::ShaderResource;
	Description.keepInitialState = true;

	FTextureResource Resource;
	Resource.Texture = Texture;
	Resource.ImageVersion = Texture->GetImageVersion();
	Resource.Handle = Context->GetDevice()->createTexture(Description);
	if (Resource.Handle == nullptr)
	{
		return nullptr;
	}

	nvrhi::ITexture* const Handle = Resource.Handle;
	Textures.insert_or_assign(Texture, std::move(Resource));
	return Handle;
}

nvrhi::ITexture* FTextureCache::GetWhiteTexture() const
{
	return WhiteTexture;
}

nvrhi::ISampler* FTextureCache::GetSampler(ETextureWrap WrapU, ETextureWrap WrapV) const
{
	return Samplers[static_cast<std::size_t>(WrapU) * WrapModeCount + static_cast<std::size_t>(WrapV)];
}

int32 FTextureCache::WritePending(nvrhi::ICommandList& CommandList)
{
	int32 WrittenCount = 0;
	for (std::pair<const WTexture* const, FTextureResource>& Entry : Textures)
	{
		FTextureResource& Resource = Entry.second;
		const WTexture* const Texture = Resource.Texture.Get();
		const bool bIsToBeWritten =
			Resource.bNeedsUpload && Texture != nullptr && Texture->GetImageVersion() == Resource.ImageVersion;
		if (!bIsToBeWritten)
		{
			continue;
		}

		const std::vector<FImage> MipChain = FMipChain::Build(Texture->GetImage(), Texture->GetColorSpace());
		for (std::size_t Mip = 0; Mip < MipChain.size(); ++Mip)
		{
			const FImage& Image = MipChain[Mip];
			const std::size_t BytesPerRow = static_cast<std::size_t>(Image.GetWidth()) * sizeof(FColor);
			CommandList.writeTexture(
				Resource.Handle, 0, static_cast<uint32>(Mip), Image.GetPixels().data(), BytesPerRow);
		}

		Resource.bNeedsUpload = false;
		++WrittenCount;
	}

	return WrittenCount;
}

void FTextureCache::ReleaseUnused()
{
	std::erase_if(Textures,
		[](const std::pair<const WTexture* const, FTextureResource>& Entry)
		{ return Entry.second.Texture.Get() == nullptr; });
}

int32 FTextureCache::GetCount() const
{
	return static_cast<int32>(Textures.size());
}
