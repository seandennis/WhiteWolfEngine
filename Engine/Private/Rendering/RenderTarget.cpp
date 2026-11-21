#include "Rendering/RenderTarget.h"

#include "Rendering/RenderContext.h"
#include "Rendering/RenderTargetImplementation.h"

#include <nvrhi/nvrhi.h>

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

// A row of pixels is copied from the graphics card straight into FColor values.
static_assert(sizeof(FColor) == 4, "FColor must be four bytes: red, green, blue, alpha");

FRenderTarget::FRenderTarget(std::unique_ptr<FImplementation> InImplementation)
	: Implementation(std::move(InImplementation))
{
}

FRenderTarget::~FRenderTarget() = default;

int32 FRenderTarget::GetWidth() const
{
	return Implementation->Width;
}

int32 FRenderTarget::GetHeight() const
{
	return Implementation->Height;
}

void FRenderTarget::Clear(const FLinearColor& Color)
{
	nvrhi::IDevice* const Device = Implementation->Context->GetDevice();
	nvrhi::ICommandList* const CommandList = Implementation->Context->GetImmediateCommandList();

	CommandList->open();
	CommandList->clearTextureFloat(
		Implementation->Texture, nvrhi::AllSubresources, nvrhi::Color(Color.R, Color.G, Color.B, Color.A));
	CommandList->close();

	Device->executeCommandList(CommandList);
	Device->runGarbageCollection();
}

bool FRenderTarget::ReadPixels(FImage& OutImage, std::string& OutError) const
{
	nvrhi::IDevice* const Device = Implementation->Context->GetDevice();
	nvrhi::ICommandList* const CommandList = Implementation->Context->GetImmediateCommandList();

	if (Implementation->ReadbackTexture == nullptr)
	{
		Implementation->ReadbackTexture =
			Device->createStagingTexture(Implementation->Texture->getDesc(), nvrhi::CpuAccessMode::Read);
		if (Implementation->ReadbackTexture == nullptr)
		{
			OutError = "the graphics card could not create the texture that pixels are read back through";
			return false;
		}
	}

	const nvrhi::TextureSlice WholeTexture;
	CommandList->open();
	CommandList->copyTexture(Implementation->ReadbackTexture, WholeTexture, Implementation->Texture, WholeTexture);
	CommandList->close();

	Device->executeCommandList(CommandList);
	if (!Device->waitForIdle())
	{
		OutError = "the graphics card did not finish drawing";
		return false;
	}
	Device->runGarbageCollection();

	std::size_t RowPitch = 0;
	const void* const Mapped =
		Device->mapStagingTexture(Implementation->ReadbackTexture, WholeTexture, nvrhi::CpuAccessMode::Read, &RowPitch);
	if (Mapped == nullptr)
	{
		OutError = "the pixels could not be read back from the graphics card";
		return false;
	}

	// Rows on the graphics card may be longer than the picture is wide, so they are copied one at a time.
	const std::size_t Width = static_cast<std::size_t>(Implementation->Width);
	const std::size_t Height = static_cast<std::size_t>(Implementation->Height);
	std::vector<FColor> Pixels(Width * Height);
	for (std::size_t Row = 0; Row < Height; ++Row)
	{
		const std::byte* const Source = static_cast<const std::byte*>(Mapped) + Row * RowPitch;
		std::memcpy(Pixels.data() + Row * Width, Source, Width * sizeof(FColor));
	}

	Device->unmapStagingTexture(Implementation->ReadbackTexture);

	OutImage = FImage(Implementation->Width, Implementation->Height, std::move(Pixels));
	return true;
}
