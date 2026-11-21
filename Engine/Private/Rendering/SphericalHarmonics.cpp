#include "Rendering/SphericalHarmonics.h"

#include "Rendering/EnvironmentMap.h"
#include "Rendering/HdrImage.h"

#include <cmath>
#include <cstddef>
#include <numbers>

namespace
{
	/** The values of the nine harmonics for a unit direction. */
	std::array<float, FSphericalHarmonics::CoefficientCount> GetBasis(const FVector3f& Direction)
	{
		const float X = Direction.x;
		const float Y = Direction.y;
		const float Z = Direction.z;
		return {
			0.282095f,
			0.488603f * Y,
			0.488603f * Z,
			0.488603f * X,
			1.092548f * X * Y,
			1.092548f * Y * Z,
			0.315392f * (3.0f * Z * Z - 1.0f),
			1.092548f * X * Z,
			0.546274f * (X * X - Y * Y),
		};
	}
}

FSphericalHarmonics FSphericalHarmonics::ProjectEnvironment(const FHdrImage& Image)
{
	FSphericalHarmonics Result;
	if (Image.IsEmpty())
	{
		return Result;
	}

	// Sums in double precision: a picture has millions of pixels, and some may be a sun thousands of times
	// brighter than the rest.
	std::array<glm::dvec3, CoefficientCount> Sums{};

	const double Pi = std::numbers::pi;
	const double RowAngle = Pi / static_cast<double>(Image.GetHeight());
	const double ColumnAngle = 2.0 * Pi / static_cast<double>(Image.GetWidth());
	for (int32 Y = 0; Y < Image.GetHeight(); ++Y)
	{
		// The part of the sphere a pixel covers shrinks towards the poles, where the rows of the picture are
		// circles of ever smaller size.
		const double Polar = (static_cast<double>(Y) + 0.5) * RowAngle;
		const double SolidAngle = std::sin(Polar) * RowAngle * ColumnAngle;
		const float Down = (static_cast<float>(Y) + 0.5f) / static_cast<float>(Image.GetHeight());

		for (int32 X = 0; X < Image.GetWidth(); ++X)
		{
			const float Across = (static_cast<float>(X) + 0.5f) / static_cast<float>(Image.GetWidth());
			const FVector3f Direction = WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(Across, Down));
			const std::array<float, CoefficientCount> Basis = GetBasis(Direction);
			const FLinearColor& Pixel = Image.GetPixel(X, Y);
			const glm::dvec3 Light(Pixel.R, Pixel.G, Pixel.B);
			for (std::size_t Index = 0; Index < Basis.size(); ++Index)
			{
				Sums[Index] += Light * (static_cast<double>(Basis[Index]) * SolidAngle);
			}
		}
	}

	for (std::size_t Index = 0; Index < Sums.size(); ++Index)
	{
		Result.Coefficients[Index] = FVector3f(Sums[Index]);
	}
	return Result;
}

FSphericalHarmonics FSphericalHarmonics::ToIrradiance() const
{
	// How much of each band a surface's cosine-weighted view of its hemisphere lets through.
	constexpr float Band0 = std::numbers::pi_v<float>;
	constexpr float Band1 = 2.0f * std::numbers::pi_v<float> / 3.0f;
	constexpr float Band2 = std::numbers::pi_v<float> / 4.0f;
	constexpr std::array<float, CoefficientCount> BandFactors{
		Band0, Band1, Band1, Band1, Band2, Band2, Band2, Band2, Band2};

	FSphericalHarmonics Result;
	for (std::size_t Index = 0; Index < Coefficients.size(); ++Index)
	{
		Result.Coefficients[Index] = Coefficients[Index] * BandFactors[Index];
	}
	return Result;
}

FVector3f FSphericalHarmonics::Evaluate(const FVector3f& Direction) const
{
	const std::array<float, CoefficientCount> Basis = GetBasis(Direction);
	FVector3f Value(0.0f);
	for (std::size_t Index = 0; Index < Basis.size(); ++Index)
	{
		Value += Coefficients[Index] * Basis[Index];
	}
	return Value;
}
