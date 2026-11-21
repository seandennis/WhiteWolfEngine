#pragma once

#include "CoreMinimal.h"

#include "Math/Vector.h"

#include <array>

class FHdrImage;

/**
 * Light from every direction, summed up in nine numbers per colour channel: the first three bands of spherical
 * harmonics. They cannot hold detail, only how the light changes broadly from one side to the other, which is
 * exactly what the light that a matte surface gathers from its whole surrounding depends on.
 */
struct FSphericalHarmonics
{
	/** The number of coefficients of three bands. */
	static constexpr int32 CoefficientCount = 9;

	/** The coefficients, red, green and blue in x, y and z, in the usual order: band 0, then band 1, then band 2. */
	std::array<FVector3f, CoefficientCount> Coefficients{};

	/**
	 * Sums up the light of a picture of a whole surrounding. See WEnvironmentMap for how such a picture maps
	 * directions to pixels.
	 *
	 * @return The harmonics of the picture's light; all zero for an empty picture.
	 */
	ENGINE_API static FSphericalHarmonics ProjectEnvironment(const FHdrImage& Image);

	/**
	 * Turns the harmonics of the light that arrives from each direction into those of the light that a surface
	 * facing each direction gathers: its irradiance. A surface gathers light from the whole half of the
	 * surrounding it faces, most from straight ahead.
	 */
	ENGINE_API FSphericalHarmonics ToIrradiance() const;

	/** Returns the value the harmonics give for a direction, which must be a unit vector. */
	ENGINE_API FVector3f Evaluate(const FVector3f& Direction) const;

	bool operator==(const FSphericalHarmonics& Other) const = default;
};
