#include "Core/Utf8.h"

#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <string>

namespace
{
	/** Returns the bytes that encode one character. */
	std::string Encode(char32_t Character)
	{
		std::string Text;
		FUtf8::AppendCharacter(Text, Character);
		return Text;
	}
}

TEST_SUITE("FUtf8")
{
	TEST_CASE("A character of the ASCII range is one byte, itself")
	{
		CHECK(Encode(U'A') == "A");
		CHECK(Encode(U'\x7F') == "\x7F");
		CHECK(Encode(U'\0') == std::string(1, '\0'));
	}

	TEST_CASE("Characters are encoded in two, three and four bytes, at both ends of each range")
	{
		CHECK(Encode(U'\x80') == "\xC2\x80");
		CHECK(Encode(U'\xE8') == "\xC3\xA8");
		CHECK(Encode(U'\x7FF') == "\xDF\xBF");

		CHECK(Encode(U'\x800') == "\xE0\xA0\x80");
		CHECK(Encode(U'\x20AC') == "\xE2\x82\xAC");
		CHECK(Encode(U'\xD7FF') == "\xED\x9F\xBF");
		CHECK(Encode(U'\xE000') == "\xEE\x80\x80");
		CHECK(Encode(U'\xFFFF') == "\xEF\xBF\xBF");

		CHECK(Encode(U'\x10000') == "\xF0\x90\x80\x80");
		CHECK(Encode(U'\x1F600') == "\xF0\x9F\x98\x80");
		CHECK(Encode(U'\x10FFFF') == "\xF4\x8F\xBF\xBF");
	}

	TEST_CASE("A character is appended to the text that is there")
	{
		std::string Text = "Caf";

		FUtf8::AppendCharacter(Text, U'\xE9');
		FUtf8::AppendCharacter(Text, U'!');

		CHECK(Text == "Caf\xC3\xA9!");
	}

	TEST_CASE("Halves of surrogate pairs and values past the last code point are not characters")
	{
		CHECK(FUtf8::IsCharacter(U'\0'));
		CHECK(FUtf8::IsCharacter(U'A'));
		CHECK(FUtf8::IsCharacter(U'\xD7FF'));
		CHECK_FALSE(FUtf8::IsCharacter(static_cast<char32_t>(0xD800)));
		CHECK_FALSE(FUtf8::IsCharacter(static_cast<char32_t>(0xDBFF)));
		CHECK_FALSE(FUtf8::IsCharacter(static_cast<char32_t>(0xDC00)));
		CHECK_FALSE(FUtf8::IsCharacter(static_cast<char32_t>(0xDFFF)));
		CHECK(FUtf8::IsCharacter(U'\xE000'));
		CHECK(FUtf8::IsCharacter(U'\x10FFFF'));
		CHECK_FALSE(FUtf8::IsCharacter(static_cast<char32_t>(0x110000)));
		CHECK_FALSE(FUtf8::IsCharacter(static_cast<char32_t>(0xFFFFFFFF)));
	}

	TEST_CASE("Appending a value that is not a character is a fatal error and leaves the text alone")
	{
		const FScopedLogCapture Capture;
		std::string Text = "Text";

		CHECK_THROWS_AS(FUtf8::AppendCharacter(Text, static_cast<char32_t>(0xD800)), FFatalErrorException);
		CHECK_THROWS_AS(FUtf8::AppendCharacter(Text, static_cast<char32_t>(0x110000)), FFatalErrorException);

		CHECK(Text == "Text");
	}
}
