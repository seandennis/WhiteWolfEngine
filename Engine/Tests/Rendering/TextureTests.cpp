#include "Rendering/Texture.h"

#include "Math/Color.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Rendering/Image.h"

#include <doctest/doctest.h>

#include <string>

TEST_SUITE("WTexture")
{
	TEST_CASE("A new texture has no picture and repeats in both directions")
	{
		const FScopedObjectTest ObjectTest;

		const WTexture* const Texture = NewObject<WTexture>();

		CHECK_FALSE(Texture->HasImage());
		CHECK(Texture->GetImage().IsEmpty());
		CHECK(Texture->GetImageVersion() == 0);
		CHECK(Texture->WrapU == ETextureWrap::Repeat);
		CHECK(Texture->WrapV == ETextureWrap::Repeat);
	}

	TEST_CASE("A texture keeps the picture and colour space it is given")
	{
		const FScopedObjectTest ObjectTest;
		WTexture* const Texture = NewObject<WTexture>();
		FImage Image(3, 2, FColor(1, 2, 3, 4));
		Image.SetPixel(2, 1, FColor(9, 8, 7, 6));
		std::string Error;

		REQUIRE(Texture->SetImage(Image, ETextureColorSpace::Linear, Error));

		CHECK(Texture->HasImage());
		CHECK(Texture->GetImage() == Image);
		CHECK(Texture->GetColorSpace() == ETextureColorSpace::Linear);

		REQUIRE(Texture->SetImage(FImage(1, 1, FColor(5, 5, 5, 5)), ETextureColorSpace::Srgb, Error));
		CHECK(Texture->GetImage() == FImage(1, 1, FColor(5, 5, 5, 5)));
		CHECK(Texture->GetColorSpace() == ETextureColorSpace::Srgb);
	}

	TEST_CASE("The image version changes with every new picture and differs between textures")
	{
		const FScopedObjectTest ObjectTest;
		WTexture* const First = NewObject<WTexture>();
		WTexture* const Second = NewObject<WTexture>();
		const FImage Image(2, 2);
		std::string Error;

		REQUIRE(First->SetImage(Image, ETextureColorSpace::Srgb, Error));
		const uint64 FirstVersion = First->GetImageVersion();
		REQUIRE(Second->SetImage(Image, ETextureColorSpace::Srgb, Error));
		const uint64 SecondVersion = Second->GetImageVersion();
		REQUIRE(First->SetImage(Image, ETextureColorSpace::Srgb, Error));

		CHECK(FirstVersion != 0);
		CHECK(SecondVersion != 0);
		CHECK(SecondVersion != FirstVersion);
		CHECK(First->GetImageVersion() != FirstVersion);
		CHECK(First->GetImageVersion() != SecondVersion);
	}

	TEST_CASE("An empty picture or one that is too large is refused, and the texture is left as it was")
	{
		const FScopedObjectTest ObjectTest;
		WTexture* const Texture = NewObject<WTexture>();
		const FImage Image(2, 2, FColor(1, 2, 3, 4));
		std::string Error;
		REQUIRE(Texture->SetImage(Image, ETextureColorSpace::Linear, Error));
		const uint64 Version = Texture->GetImageVersion();

		CHECK_FALSE(Texture->SetImage(FImage(), ETextureColorSpace::Srgb, Error));
		CHECK(Error == "a texture cannot be 0 by 0 pixels; each side must be between 1 and 16384");

		CHECK_FALSE(Texture->SetImage(FImage(WTexture::MaxDimension + 1, 1), ETextureColorSpace::Srgb, Error));
		CHECK(Error == "a texture cannot be 16385 by 1 pixels; each side must be between 1 and 16384");

		CHECK_FALSE(Texture->SetImage(FImage(1, WTexture::MaxDimension + 1), ETextureColorSpace::Srgb, Error));

		CHECK(Texture->GetImage() == Image);
		CHECK(Texture->GetColorSpace() == ETextureColorSpace::Linear);
		CHECK(Texture->GetImageVersion() == Version);
	}
}
