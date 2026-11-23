#include "Ui/UiText.h"

#include <doctest/doctest.h>

TEST_SUITE("FUiText")
{
	TEST_CASE("SplitWords puts a space where a capital begins a word")
	{
		CHECK(FUiText::SplitWords("CastShadows") == "Cast Shadows");
		CHECK(FUiText::SplitWords("RelativeTransform") == "Relative Transform");
		CHECK(FUiText::SplitWords("metallicRoughnessTexture") == "metallic Roughness Texture");
		CHECK(FUiText::SplitWords("Intensity") == "Intensity");
	}

	TEST_CASE("SplitWords keeps a run of capitals together, and ends it where a word in small letters begins")
	{
		CHECK(FUiText::SplitWords("HDRImage") == "HDR Image");
		CHECK(FUiText::SplitWords("EntityID") == "Entity ID");
		CHECK(FUiText::SplitWords("UVScale") == "UV Scale");
		CHECK(FUiText::SplitWords("UV") == "UV");
		CHECK(FUiText::SplitWords("X") == "X");
	}

	TEST_CASE("SplitWords sets a number apart, with the capitals that belong to it")
	{
		CHECK(FUiText::SplitWords("Level2Boss") == "Level 2 Boss");
		CHECK(FUiText::SplitWords("Vector3D") == "Vector 3D");
		CHECK(FUiText::SplitWords("Slot12") == "Slot 12");
		CHECK(FUiText::SplitWords("2ndPass") == "2nd Pass");
	}

	TEST_CASE("SplitWords leaves alone what is words already, and what it has no rule for")
	{
		CHECK(FUiText::SplitWords("") == "");
		CHECK(FUiText::SplitWords("Point Light") == "Point Light");
		CHECK(FUiText::SplitWords("snake_case_name") == "snake case name");
		CHECK(FUiText::SplitWords("lowercase") == "lowercase");
		// Letters beyond A to Z are neither capitals nor small letters here: nothing is put between them.
		CHECK(FUiText::SplitWords("Größe") == "Größe");
	}

	TEST_CASE("MakePropertyLabel leaves out the b of a boolean")
	{
		CHECK(FUiText::MakePropertyLabel("bCastShadows") == "Cast Shadows");
		CHECK(FUiText::MakePropertyLabel("bVisible") == "Visible");
		CHECK(FUiText::MakePropertyLabel("BaseColor") == "Base Color");

		// A b is the prefix only before a capital.
		CHECK(FUiText::MakePropertyLabel("ballSpeed") == "ball Speed");
		CHECK(FUiText::MakePropertyLabel("b") == "b");
		CHECK(FUiText::MakePropertyLabel("") == "");
	}

	TEST_CASE("MakeClassLabel leaves out the letter that says what kind of type a class is")
	{
		CHECK(FUiText::MakeClassLabel("WPointLightComponent") == "Point Light Component");
		CHECK(FUiText::MakeClassLabel("WEntity") == "Entity");
		CHECK(FUiText::MakeClassLabel("FTransform") == "Transform");
		CHECK(FUiText::MakeClassLabel("WHDRImage") == "HDR Image");

		// A first letter is the prefix only before a capital, and only if it is one of the prefixes.
		CHECK(FUiText::MakeClassLabel("Widget") == "Widget");
		CHECK(FUiText::MakeClassLabel("XRayComponent") == "X Ray Component");
		CHECK(FUiText::MakeClassLabel("W") == "W");
		CHECK(FUiText::MakeClassLabel("") == "");
	}

	TEST_CASE("ContainsIgnoringCase finds a part whatever its capitals")
	{
		CHECK(FUiText::ContainsIgnoringCase("Point Light", "light"));
		CHECK(FUiText::ContainsIgnoringCase("Point Light", "POINT"));
		CHECK(FUiText::ContainsIgnoringCase("Point Light", "t L"));
		CHECK(FUiText::ContainsIgnoringCase("Point Light", "Point Light"));
		CHECK(FUiText::ContainsIgnoringCase("Point Light", ""));
		CHECK(FUiText::ContainsIgnoringCase("", ""));

		CHECK_FALSE(FUiText::ContainsIgnoringCase("Point Light", "Lights"));
		CHECK_FALSE(FUiText::ContainsIgnoringCase("Point Light", "PointLight"));
		CHECK_FALSE(FUiText::ContainsIgnoringCase("", "a"));
	}
}
