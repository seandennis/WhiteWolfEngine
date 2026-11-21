#include "Rendering/EnvironmentMap.h"

#include <glm/common.hpp>

#include <cmath>
#include <format>
#include <numbers>
#include <utility>
#include <vector>

namespace
{
	/** The version given to the picture set last. Objects are used on the main thread only. */
	constinit uint64 LastImageVersion = 0;

	float SanitizeLight(float Value)
	{
		return std::isfinite(Value) && Value > 0.0f ? Value : 0.0f;
	}
}

bool WEnvironmentMap::SetImage(FHdrImage InImage, std::string& OutError)
{
	const bool bIsValidSize =
		!InImage.IsEmpty() && InImage.GetWidth() <= MaxDimension && InImage.GetHeight() <= MaxDimension;
	if (!bIsValidSize)
	{
		OutError = std::format("an environment map cannot be {} by {} pixels; each side must be between 1 and {}",
			InImage.GetWidth(),
			InImage.GetHeight(),
			MaxDimension);
		return false;
	}

	std::vector<FLinearColor> Pixels = InImage.GetPixels();
	for (FLinearColor& Pixel : Pixels)
	{
		Pixel = FLinearColor(SanitizeLight(Pixel.R), SanitizeLight(Pixel.G), SanitizeLight(Pixel.B));
	}

	Image = FHdrImage(InImage.GetWidth(), InImage.GetHeight(), std::move(Pixels));
	Irradiance = FSphericalHarmonics::ProjectEnvironment(Image).ToIrradiance();
	++LastImageVersion;
	ImageVersion = LastImageVersion;
	return true;
}

bool WEnvironmentMap::HasImage() const
{
	return ImageVersion != 0;
}

const FHdrImage& WEnvironmentMap::GetImage() const
{
	return Image;
}

uint64 WEnvironmentMap::GetImageVersion() const
{
	return ImageVersion;
}

const FSphericalHarmonics& WEnvironmentMap::GetIrradiance() const
{
	return Irradiance;
}

FVector2f WEnvironmentMap::GetCoordinateOfDirection(const FVector3f& Direction)
{
	// The angle to the right of straight ahead, and the angle down from straight up.
	const float Azimuth = std::atan2(Direction.x, -Direction.z);
	const float Polar = std::acos(glm::clamp(Direction.y, -1.0f, 1.0f));
	return FVector2f(0.5f + Azimuth / (2.0f * std::numbers::pi_v<float>), Polar / std::numbers::pi_v<float>);
}

FVector3f WEnvironmentMap::GetDirectionOfCoordinate(const FVector2f& Coordinate)
{
	const float Azimuth = (Coordinate.x - 0.5f) * 2.0f * std::numbers::pi_v<float>;
	const float Polar = Coordinate.y * std::numbers::pi_v<float>;
	const float Level = std::sin(Polar);
	return FVector3f(Level * std::sin(Azimuth), std::cos(Polar), -Level * std::cos(Azimuth));
}
