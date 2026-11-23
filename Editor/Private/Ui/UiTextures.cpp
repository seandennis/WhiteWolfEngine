#include "Ui/UiTextures.h"

#include "Math/Color.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/Image.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureColorSpace.h"
#include "Rendering/TextureWrap.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <utility>

namespace
{
	/** Identifiers from here on are pictures of the current frame; the ones below are textures of Dear ImGui. */
	constexpr ImTextureID FirstFramePicture = ImTextureID(1) << 32;

	// Dear ImGui's pixels are copied into images as they are: four bytes in the order red, green, blue, alpha.
	static_assert(sizeof(FColor) == 4);
}

FUiTextures::~FUiTextures()
{
	for (const TWeakObjectPtr<WTexture>& Texture : OwnedTextures)
	{
		WW_CHECKF(!Texture.IsValid(), "A texture of the user interface was not released before its owner went away");
	}
}

void FUiTextures::BeginFrame()
{
	FramePictures.clear();
}

ImTextureID FUiTextures::AddRenderTarget(const FRenderTarget& Target)
{
	FUiPicture Picture;
	Picture.RenderTarget = &Target;
	FramePictures.push_back(Picture);
	return FirstFramePicture + static_cast<ImTextureID>(FramePictures.size() - 1);
}

bool FUiTextures::Update(ImVector<ImTextureData*>& UiTextures, std::string& OutError)
{
	for (ImTextureData* const UiTexture : UiTextures)
	{
		if (UiTexture->Status == ImTextureStatus_WantCreate)
		{
			WTexture* const Texture = NewObject<WTexture>();
			Texture->WrapU = ETextureWrap::ClampToEdge;
			Texture->WrapV = ETextureWrap::ClampToEdge;
			if (!CopyPixels(*UiTexture, *Texture, OutError))
			{
				return false;
			}

			// Nothing refers to the texture but the numbers in Dear ImGui's lists, so this is what keeps it.
			Texture->AddToRoot();

			std::size_t Slot = 0;
			while (Slot < OwnedTextures.size() && OwnedTextures[Slot].IsValid())
			{
				++Slot;
			}
			if (Slot == OwnedTextures.size())
			{
				OwnedTextures.emplace_back();
			}
			OwnedTextures[Slot] = Texture;

			UiTexture->SetTexID(static_cast<ImTextureID>(Slot + 1));
			UiTexture->SetStatus(ImTextureStatus_OK);
		}
		else if (UiTexture->Status == ImTextureStatus_WantUpdates)
		{
			// Dear ImGui says which parts are new; a texture takes a whole picture, so it gets all of it again.
			WTexture* const Texture = FindOwnedTexture(UiTexture->GetTexID());
			WW_CHECKF(Texture != nullptr, "Dear ImGui updates a texture that the user interface does not have");
			if (!CopyPixels(*UiTexture, *Texture, OutError))
			{
				return false;
			}
			UiTexture->SetStatus(ImTextureStatus_OK);
		}
		else if (UiTexture->Status == ImTextureStatus_WantDestroy && UiTexture->UnusedFrames > 0)
		{
			Destroy(*UiTexture);
		}
	}
	return true;
}

void FUiTextures::ReleaseAll(ImVector<ImTextureData*>& UiTextures)
{
	for (ImTextureData* const UiTexture : UiTextures)
	{
		if (UiTexture->GetTexID() != ImTextureID_Invalid)
		{
			Destroy(*UiTexture);
		}
	}
}

FUiPicture FUiTextures::Find(ImTextureID Identifier) const
{
	FUiPicture Picture;
	if (Identifier >= FirstFramePicture)
	{
		const ImTextureID Index = Identifier - FirstFramePicture;
		if (Index < FramePictures.size())
		{
			Picture = FramePictures[static_cast<std::size_t>(Index)];
		}
	}
	else
	{
		Picture.Texture = FindOwnedTexture(Identifier);
	}
	return Picture;
}

WTexture* FUiTextures::FindOwnedTexture(ImTextureID Identifier) const
{
	const bool bIsOwned = Identifier != ImTextureID_Invalid && Identifier <= OwnedTextures.size();
	return bIsOwned ? OwnedTextures[static_cast<std::size_t>(Identifier - 1)].Get() : nullptr;
}

bool FUiTextures::CopyPixels(const ImTextureData& UiTexture, WTexture& Texture, std::string& OutError) const
{
	WW_CHECKF(UiTexture.Format == ImTextureFormat_RGBA32 && UiTexture.Pixels != nullptr,
		"Dear ImGui has a texture in a format that the user interface did not ask for");

	const std::size_t PixelCount =
		static_cast<std::size_t>(UiTexture.Width) * static_cast<std::size_t>(UiTexture.Height);
	std::vector<FColor> Pixels(PixelCount);
	std::memcpy(Pixels.data(), UiTexture.Pixels, PixelCount * sizeof(FColor));

	// What the texture holds are colours, and where it is white, as for the shapes of characters, the colour space
	// makes no difference.
	std::string Reason;
	if (!Texture.SetImage(
			FImage(UiTexture.Width, UiTexture.Height, std::move(Pixels)), ETextureColorSpace::Srgb, Reason))
	{
		OutError = std::format("a texture of the user interface, of {} by {} pixels, could not be made: {}",
			UiTexture.Width,
			UiTexture.Height,
			Reason);
		return false;
	}
	return true;
}

void FUiTextures::Destroy(ImTextureData& UiTexture)
{
	const ImTextureID Identifier = UiTexture.GetTexID();
	WW_CHECKF(Identifier != ImTextureID_Invalid && Identifier <= OwnedTextures.size(),
		"Dear ImGui destroys a texture that the user interface does not have");

	TWeakObjectPtr<WTexture>& Slot = OwnedTextures[static_cast<std::size_t>(Identifier - 1)];
	if (WTexture* const Texture = Slot.Get())
	{
		Texture->RemoveFromRoot();
		Texture->Destroy();
	}
	Slot = nullptr;

	UiTexture.SetTexID(ImTextureID_Invalid);
	UiTexture.SetStatus(ImTextureStatus_Destroyed);
}
