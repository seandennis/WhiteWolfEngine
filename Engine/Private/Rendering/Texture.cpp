#include "Rendering/Texture.h"

#include <format>
#include <utility>

namespace
{
	/** The version given to the picture set last. Objects are used on the main thread only. */
	constinit uint64 LastImageVersion = 0;
}

bool WTexture::SetImage(FImage InImage, ETextureColorSpace InColorSpace, std::string& OutError)
{
	const bool bIsValidSize =
		!InImage.IsEmpty() && InImage.GetWidth() <= MaxDimension && InImage.GetHeight() <= MaxDimension;
	if (!bIsValidSize)
	{
		OutError = std::format("a texture cannot be {} by {} pixels; each side must be between 1 and {}",
			InImage.GetWidth(),
			InImage.GetHeight(),
			MaxDimension);
		return false;
	}

	Image = std::move(InImage);
	ColorSpace = InColorSpace;
	++LastImageVersion;
	ImageVersion = LastImageVersion;
	return true;
}

bool WTexture::HasImage() const
{
	return ImageVersion != 0;
}

const FImage& WTexture::GetImage() const
{
	return Image;
}

ETextureColorSpace WTexture::GetColorSpace() const
{
	return ColorSpace;
}

uint64 WTexture::GetImageVersion() const
{
	return ImageVersion;
}
