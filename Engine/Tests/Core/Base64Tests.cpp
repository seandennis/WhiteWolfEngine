#include "Core/Base64.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Returns the bytes of text. */
	std::vector<std::byte> ToBytes(std::string_view Text)
	{
		std::vector<std::byte> Bytes;
		for (const char Character : Text)
		{
			Bytes.push_back(static_cast<std::byte>(Character));
		}
		return Bytes;
	}

	/** Returns the text that base64 stands for, or "(not base64)". */
	std::string DecodeToText(std::string_view Encoded)
	{
		const std::optional<std::vector<std::byte>> Bytes = FBase64::Decode(Encoded);
		if (!Bytes.has_value())
		{
			return "(not base64)";
		}
		std::string Text;
		for (const std::byte Byte : *Bytes)
		{
			Text += static_cast<char>(Byte);
		}
		return Text;
	}
}

TEST_SUITE("FBase64")
{
	TEST_CASE("Encode and Decode turn the examples of RFC 4648 into each other")
	{
		struct FExample
		{
			std::string_view Text;
			std::string_view Encoded;
		};

		for (const FExample& Example : {FExample{"", ""},
				 FExample{"f", "Zg=="},
				 FExample{"fo", "Zm8="},
				 FExample{"foo", "Zm9v"},
				 FExample{"foob", "Zm9vYg=="},
				 FExample{"fooba", "Zm9vYmE="},
				 FExample{"foobar", "Zm9vYmFy"}})
		{
			CAPTURE(Example.Text);
			CHECK(FBase64::Encode(ToBytes(Example.Text)) == Example.Encoded);
			CHECK(DecodeToText(Example.Encoded) == Example.Text);
		}
	}

	TEST_CASE("Every byte value comes back as it was")
	{
		std::vector<std::byte> Bytes;
		for (int32 Value = 0; Value < 256; ++Value)
		{
			Bytes.push_back(static_cast<std::byte>(Value));
		}

		const std::string Encoded = FBase64::Encode(Bytes);

		CHECK(Encoded.size() == 344);
		CHECK(FBase64::Decode(Encoded) == Bytes);
	}

	TEST_CASE("Decode reads the alphabet of addresses and text without padding, and ignores what follows padding")
	{
		// 0xFB 0xFF is "+/8=" in the standard alphabet.
		const std::vector<std::byte> Expected = {std::byte{0xFB}, std::byte{0xFF}};

		CHECK(FBase64::Decode("+/8=") == Expected);
		CHECK(FBase64::Decode("-_8=") == Expected);
		CHECK(FBase64::Decode("-_8") == Expected);
		CHECK(DecodeToText("Zm8=anything at all") == "fo");
	}

	TEST_CASE("Decode refuses text with a character that is in neither alphabet")
	{
		CHECK_FALSE(FBase64::Decode("Zm*v").has_value());
		CHECK_FALSE(FBase64::Decode("Zm9v Zm9v").has_value());
		CHECK_FALSE(FBase64::Decode("Zm9v\n").has_value());
	}
}
