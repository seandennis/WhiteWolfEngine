#include "Core/Base64.h"

namespace
{
	/** The standard alphabet, in which a letter's place is the six bits it stands for. */
	constexpr std::string_view Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string FBase64::Encode(std::span<const std::byte> Bytes)
{
	std::string Text;
	Text.reserve((Bytes.size() + 2) / 3 * 4);
	for (std::size_t Index = 0; Index < Bytes.size(); Index += 3)
	{
		// Three bytes are four letters of six bits each; a last group of fewer bytes is padded with '='.
		const std::size_t Remaining = Bytes.size() - Index;
		const uint32 First = std::to_integer<uint32>(Bytes[Index]);
		const uint32 Second = Remaining > 1 ? std::to_integer<uint32>(Bytes[Index + 1]) : 0;
		const uint32 Third = Remaining > 2 ? std::to_integer<uint32>(Bytes[Index + 2]) : 0;
		const uint32 Group = (First << 16) | (Second << 8) | Third;

		Text += Alphabet[(Group >> 18) & 63];
		Text += Alphabet[(Group >> 12) & 63];
		Text += Remaining > 1 ? Alphabet[(Group >> 6) & 63] : '=';
		Text += Remaining > 2 ? Alphabet[Group & 63] : '=';
	}
	return Text;
}

std::optional<std::vector<std::byte>> FBase64::Decode(std::string_view Text)
{
	std::vector<std::byte> Bytes;
	Bytes.reserve(Text.size() / 4 * 3);

	uint32 Accumulator = 0;
	int32 BitCount = 0;
	for (const char Character : Text)
	{
		uint32 Value = 0;
		if (Character >= 'A' && Character <= 'Z')
		{
			Value = static_cast<uint32>(Character - 'A');
		}
		else if (Character >= 'a' && Character <= 'z')
		{
			Value = static_cast<uint32>(Character - 'a') + 26;
		}
		else if (Character >= '0' && Character <= '9')
		{
			Value = static_cast<uint32>(Character - '0') + 52;
		}
		else if (Character == '+' || Character == '-')
		{
			Value = 62;
		}
		else if (Character == '/' || Character == '_')
		{
			Value = 63;
		}
		else if (Character == '=')
		{
			// Padding: the data is complete.
			break;
		}
		else
		{
			return std::nullopt;
		}

		Accumulator = (Accumulator << 6) | Value;
		BitCount += 6;
		if (BitCount >= 8)
		{
			BitCount -= 8;
			Bytes.push_back(static_cast<std::byte>((Accumulator >> BitCount) & 0xffu));
		}
	}

	return Bytes;
}
