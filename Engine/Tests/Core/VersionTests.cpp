#include "Core/Version.h"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <string_view>

TEST_SUITE("FVersion")
{
	TEST_CASE("A default-constructed version is 0.0.0")
	{
		const FVersion Version;

		CHECK(Version.Major == 0);
		CHECK(Version.Minor == 0);
		CHECK(Version.Patch == 0);
	}

	TEST_CASE("Parse accepts well-formed versions")
	{
		struct FCase
		{
			std::string_view Text;
			FVersion Expected;
		};

		const FCase Cases[] = {
			{"0.0.0", {0, 0, 0}},
			{"0.1.0", {0, 1, 0}},
			{"1.2.3", {1, 2, 3}},
			{"10.20.30", {10, 20, 30}},
			{"2147483647.2147483647.2147483647", {2147483647, 2147483647, 2147483647}},
		};

		for (const FCase& Case : Cases)
		{
			CAPTURE(Case.Text);

			const std::optional<FVersion> Parsed = FVersion::Parse(Case.Text);

			REQUIRE(Parsed.has_value());
			CHECK(Parsed->Major == Case.Expected.Major);
			CHECK(Parsed->Minor == Case.Expected.Minor);
			CHECK(Parsed->Patch == Case.Expected.Patch);
		}
	}

	TEST_CASE("Parse rejects malformed versions")
	{
		const std::string_view Cases[] = {
			"",
			".",
			"..",
			"1",
			"1.",
			"1.2",
			"1.2.",
			"1.2.3.",
			"1.2.3.4",
			".1.2",
			"1..3",
			"a.b.c",
			"1.2.x",
			"1.2.3x",
			"x1.2.3",
			"-1.2.3",
			"1.-2.3",
			"+1.2.3",
			" 1.2.3",
			"1.2.3 ",
			"1. 2.3",
			"01.2.3",
			"1.02.3",
			"1.2.03",
			"00.0.0",
			"1.2.3-beta",
			"1,2,3",
			"2147483648.0.0",
			"0.0.99999999999999999999",
		};

		for (const std::string_view Case : Cases)
		{
			CAPTURE(Case);

			CHECK_FALSE(FVersion::Parse(Case).has_value());
		}
	}

	TEST_CASE("Parse does not read past the end of the view")
	{
		const std::string_view Buffer = "1.2.34";

		const std::optional<FVersion> Parsed = FVersion::Parse(Buffer.substr(0, 5));

		REQUIRE(Parsed.has_value());
		CHECK(Parsed->Patch == 3);
	}

	TEST_CASE("ToString formats as Major.Minor.Patch")
	{
		CHECK(FVersion{}.ToString() == "0.0.0");
		CHECK(FVersion{1, 2, 3}.ToString() == "1.2.3");
		CHECK(FVersion{2147483647, 0, 10}.ToString() == "2147483647.0.10");
	}

	TEST_CASE("Parse and ToString round-trip")
	{
		const std::string Text = "12.0.345";

		const std::optional<FVersion> Parsed = FVersion::Parse(Text);

		REQUIRE(Parsed.has_value());
		CHECK(Parsed->ToString() == Text);
	}

	TEST_CASE("Versions order by major, then minor, then patch")
	{
		CHECK(FVersion{1, 2, 3} == FVersion{1, 2, 3});
		CHECK(FVersion{1, 2, 3} != FVersion{1, 2, 4});

		CHECK(FVersion{1, 2, 3} < FVersion{1, 2, 4});
		CHECK(FVersion{1, 2, 9} < FVersion{1, 3, 0});
		CHECK(FVersion{1, 9, 9} < FVersion{2, 0, 0});
		CHECK(FVersion{2, 0, 0} > FVersion{1, 9, 9});
		CHECK(FVersion{1, 2, 3} <= FVersion{1, 2, 3});
		CHECK(FVersion{1, 2, 3} >= FVersion{1, 2, 3});

		// A larger minor or patch never outranks a larger major.
		CHECK(FVersion{1, 100, 100} < FVersion{2, 0, 0});
		CHECK(FVersion{1, 2, 100} < FVersion{1, 3, 0});
	}

	TEST_CASE("Comparison is usable in constant expressions")
	{
		static_assert(FVersion{1, 2, 3} < FVersion{1, 2, 4});
		static_assert(FVersion{1, 2, 3} == FVersion{1, 2, 3});
	}
}
