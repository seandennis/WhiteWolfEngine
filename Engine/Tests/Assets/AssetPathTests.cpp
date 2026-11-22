#include "Assets/AssetPath.h"

#include <doctest/doctest.h>

#include <optional>
#include <ostream>
#include <string>

TEST_SUITE("FAssetPath")
{
	TEST_CASE("A path without a key names the asset of a file")
	{
		std::string Error;

		const std::optional<FAssetPath> Path = FAssetPath::Parse("Textures/Wood.png", Error);

		REQUIRE(Path.has_value());
		CHECK(Path->File == "Textures/Wood.png");
		CHECK(Path->Key.empty());
		CHECK(Path->ToString() == "Textures/Wood.png");
	}

	TEST_CASE("A path with a key names one asset of a file that holds several")
	{
		std::string Error;

		const std::optional<FAssetPath> Path = FAssetPath::Parse("Models/Old Crate.glb#Meshes/0/Primitives/1", Error);

		REQUIRE(Path.has_value());
		CHECK(Path->File == "Models/Old Crate.glb");
		CHECK(Path->Key == "Meshes/0/Primitives/1");
		CHECK(Path->ToString() == "Models/Old Crate.glb#Meshes/0/Primitives/1");
	}

	TEST_CASE("A file at the top of the content directory, and a key that is empty, are allowed")
	{
		std::string Error;

		REQUIRE(FAssetPath::Parse("Sky.hdr", Error).has_value());
		const std::optional<FAssetPath> Path = FAssetPath::Parse("Sky.hdr#", Error);
		REQUIRE(Path.has_value());
		CHECK(Path->Key.empty());
		CHECK(Path->ToString() == "Sky.hdr");
	}

	TEST_CASE("Text that could lead out of the content directory, or names no file, is not an asset path")
	{
		std::string Text;
		std::string ExpectedReason;

		SUBCASE("Nothing")
		{
			Text = "";
			ExpectedReason = "it names no file";
		}
		SUBCASE("A key without a file")
		{
			Text = "#Materials/0";
			ExpectedReason = "it names no file";
		}
		SUBCASE("A path from the root")
		{
			Text = "/Textures/Wood.png";
			ExpectedReason = "it must be relative to the content directory";
		}
		SUBCASE("A path with a drive")
		{
			Text = "C:/Textures/Wood.png";
			ExpectedReason = "it must be relative to the content directory, without a drive";
		}
		SUBCASE("Backslashes")
		{
			Text = "Textures\\Wood.png";
			ExpectedReason = "its parts must be separated by forward slashes";
		}
		SUBCASE("The directory above")
		{
			Text = "../Secrets/Key.png";
			ExpectedReason = "it must not have the part \"..\"";
		}
		SUBCASE("The directory above, further in")
		{
			Text = "Textures/../../Key.png";
			ExpectedReason = "it must not have the part \"..\"";
		}
		SUBCASE("The directory itself")
		{
			Text = "./Wood.png";
			ExpectedReason = "it must not have the part \".\"";
		}
		SUBCASE("Two slashes")
		{
			Text = "Textures//Wood.png";
			ExpectedReason = "it has an empty part between or after its slashes";
		}
		SUBCASE("A slash at the end")
		{
			Text = "Textures/";
			ExpectedReason = "it has an empty part between or after its slashes";
		}
		SUBCASE("Two keys")
		{
			Text = "Models/Crate.glb#Meshes/0#Materials/0";
			ExpectedReason = "it has more than one \"#\"";
		}

		std::string Error;
		const std::optional<FAssetPath> Path = FAssetPath::Parse(Text, Error);

		CHECK_FALSE(Path.has_value());
		CHECK(Error == "\"" + Text + "\" is not the path of an asset: " + ExpectedReason);
	}

	TEST_CASE("Two paths are equal if their files and keys are")
	{
		CHECK(FAssetPath{"A.png", ""} == FAssetPath{"A.png", ""});
		CHECK_FALSE(FAssetPath{"A.png", ""} == FAssetPath{"A.png", "Linear"});
		CHECK_FALSE(FAssetPath{"A.png", ""} == FAssetPath{"a.png", ""});
	}
}
