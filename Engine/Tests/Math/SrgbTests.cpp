#include "Math/Srgb.h"

#include <doctest/doctest.h>

#include <cmath>
#include <initializer_list>

TEST_SUITE("FSrgb")
{
	TEST_CASE("Black and white are encoded as themselves")
	{
		CHECK(FSrgb::Encode(0.0f) == 0.0f);
		CHECK(FSrgb::Decode(0.0f) == 0.0f);
		CHECK(FSrgb::Encode(1.0f) == doctest::Approx(1.0f));
		CHECK(FSrgb::Decode(1.0f) == doctest::Approx(1.0f));
	}

	TEST_CASE("The values of the standard come out")
	{
		// The middle grey of a display, 128 of 255, is a little more than a fifth of the light of white.
		CHECK(FSrgb::Decode(128.0f / 255.0f) == doctest::Approx(0.21586f).epsilon(0.0001));
		CHECK(FSrgb::Encode(0.21586f) == doctest::Approx(128.0f / 255.0f).epsilon(0.0001));
		CHECK(FSrgb::Encode(0.5f) == doctest::Approx(0.73536f).epsilon(0.0001));

		// Near black the encoding is a straight line.
		CHECK(FSrgb::Encode(0.001f) == doctest::Approx(0.01292f).epsilon(0.0001));
		CHECK(FSrgb::Decode(0.02f) == doctest::Approx(0.02f / 12.92f).epsilon(0.0001));
	}

	TEST_CASE("The straight piece and the curve meet")
	{
		// Just below and just above where one ends and the other begins, the values are next to each other.
		const float Below = FSrgb::Encode(0.0031307f);
		const float Above = FSrgb::Encode(0.0031309f);

		CHECK(Above > Below);
		CHECK(Above - Below < 0.0001f);
	}

	TEST_CASE("Decoding gives back what was encoded")
	{
		for (const float Linear : {0.0f, 0.0001f, 0.002f, 0.0031308f, 0.01f, 0.18f, 0.5f, 0.9f, 1.0f, 4.0f, 100.0f})
		{
			CAPTURE(Linear);
			CHECK(FSrgb::Decode(FSrgb::Encode(Linear)) == doctest::Approx(Linear).epsilon(0.0001));
		}
	}

	TEST_CASE("More light is a greater value, also beyond white")
	{
		float Previous = FSrgb::Encode(0.0f);
		for (const float Linear : {0.001f, 0.0031308f, 0.004f, 0.1f, 0.5f, 1.0f, 2.0f, 16.0f})
		{
			CAPTURE(Linear);
			const float Encoded = FSrgb::Encode(Linear);
			CHECK(Encoded > Previous);
			Previous = Encoded;
		}
		CHECK(FSrgb::Encode(4.0f) > 1.0f);
	}

	TEST_CASE("A negative amount is encoded as its positive is, with its sign")
	{
		CHECK(FSrgb::Encode(-0.5f) == -FSrgb::Encode(0.5f));
		CHECK(FSrgb::Decode(-0.5f) == -FSrgb::Decode(0.5f));
		CHECK(FSrgb::Encode(-0.001f) == -FSrgb::Encode(0.001f));
	}
}
