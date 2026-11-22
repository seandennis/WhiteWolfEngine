#pragma once

#include "CoreMinimal.h"

#include "Assets/Asset.h"
#include "Math/Vector.h"
#include "Object/ObjectMacros.h"
#include "Rendering/HdrImage.h"
#include "Rendering/SphericalHarmonics.h"

#include <string>

/**
 * The light of a whole surrounding: a sky, a studio, a street. It is a picture of light that shows every
 * direction at once, as HDRIs do, for example those of polyhaven.com. A sky light lights a scene with it.
 *
 * The picture is a map of directions like a map of the world: its width is the full turn around the vertical
 * axis and its height the half turn from straight up to straight down. The middle of the picture is straight
 * ahead (-Z), the right half is to the right (+X), the left and right edges meet behind (+Z), the top row is up
 * (+Y) and the bottom row is down. Such a picture is twice as wide as it is high.
 */
WCLASS()
class ENGINE_API WEnvironmentMap : public WAsset
{
	WW_DECLARE_CLASS(WEnvironmentMap, WAsset)

public:
	/** The largest width or height the picture can have, in pixels. */
	static constexpr int32 MaxDimension = 16384;

	/**
	 * Replaces the picture.
	 *
	 * Values that are not amounts of light, negative or not finite, are taken as no light.
	 *
	 * @param OutError Receives the reason when the picture is refused: it is empty, or a side is longer than
	 * MaxDimension.
	 * @return True if the environment now has the given picture. Otherwise it is left as it was.
	 */
	bool SetImage(FHdrImage InImage, std::string& OutError);

	/** Returns true once the environment has been given a picture. One without a picture gives no light. */
	bool HasImage() const;

	const FHdrImage& GetImage() const;

	/**
	 * Returns a number that identifies the picture the environment has now. It changes every time SetImage
	 * succeeds and is never the same for two environments. Zero means there is no picture.
	 */
	uint64 GetImageVersion() const;

	/**
	 * Returns the light a matte surface gathers from the environment, for every direction the surface can face:
	 * the irradiance, as spherical harmonics. All zero while there is no picture.
	 */
	const FSphericalHarmonics& GetIrradiance() const;

	/**
	 * Returns where in the picture a direction is: (0, 0) is the top left corner and (1, 1) the bottom right.
	 *
	 * @param Direction A unit vector. Straight up and straight down are the whole top and bottom rows.
	 */
	static FVector2f GetCoordinateOfDirection(const FVector3f& Direction);

	/** Returns the direction, a unit vector, that a place in the picture shows. The inverse of
	 * GetCoordinateOfDirection. */
	static FVector3f GetDirectionOfCoordinate(const FVector2f& Coordinate);

private:
	FHdrImage Image;
	FSphericalHarmonics Irradiance;
	uint64 ImageVersion = 0;
};
