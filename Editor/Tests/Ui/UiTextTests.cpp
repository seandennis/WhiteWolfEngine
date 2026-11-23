#include "Ui/UiText.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

namespace
{
	/** A measure for FitEnd under which every byte of a text is one wide. */
	float MeasureBytes(std::string_view Text)
	{
		return static_cast<float>(Text.size());
	}
}

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

	TEST_CASE("FitEnd returns a text that fits as it is")
	{
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 15.0f, MeasureBytes) == "C:/Games/Tetris");
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 400.0f, MeasureBytes) == "C:/Games/Tetris");
		CHECK(FUiText::FitEnd("", 0.0f, MeasureBytes) == "");
	}

	TEST_CASE("FitEnd keeps the longest end of a text that fits after the dots")
	{
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 14.0f, MeasureBytes) == "...ames/Tetris");
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 10.0f, MeasureBytes) == ".../Tetris");
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 10.9f, MeasureBytes) == ".../Tetris");
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 4.0f, MeasureBytes) == "...s");
	}

	TEST_CASE("FitEnd returns the dots alone where nothing fits beside them")
	{
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 3.0f, MeasureBytes) == "...");
		CHECK(FUiText::FitEnd("C:/Games/Tetris", 0.0f, MeasureBytes) == "...");
		CHECK(FUiText::FitEnd("C:/Games/Tetris", -1.0f, MeasureBytes) == "...");
	}

	TEST_CASE("FitEnd leaves a character of several bytes out whole")
	{
		// "wxyz", then the letter e with an acute accent, which is two bytes in UTF-8, then "ab".
		const std::string Text = "wxyz\xC3\xA9"
								 "ab";

		// There is room for three bytes after the dots, and the last three are half of the accented letter and
		// "ab". Half a letter is no character: the letter is left out whole.
		CHECK(FUiText::FitEnd(Text, 6.0f, MeasureBytes) == "...ab");
		// With room for one more byte the letter is there whole.
		CHECK(FUiText::FitEnd(Text, 7.0f, MeasureBytes) ==
			"...\xC3\xA9"
			"ab");
	}

	TEST_CASE("FitEnd measures what it is about to return, dots and all")
	{
		// Every letter is one wide, and a dot is two.
		const auto MeasureWideDots = [](std::string_view Text)
		{
			float Width = 0.0f;
			for (const char Character : Text)
			{
				Width += Character == '.' ? 2.0f : 1.0f;
			}
			return Width;
		};

		// The three dots are six wide, which leaves room for six letters in twelve, not for nine.
		CHECK(FUiText::FitEnd("Projects/Tetris", 12.0f, MeasureWideDots) == "...Tetris");
		CHECK(FUiText::FitEnd("Projects/Tetris", 15.0f, MeasureWideDots) == "Projects/Tetris");
		// Where the dots alone are too wide, they are still what is returned.
		CHECK(FUiText::FitEnd("Projects/Tetris", 5.0f, MeasureWideDots) == "...");
	}
}
