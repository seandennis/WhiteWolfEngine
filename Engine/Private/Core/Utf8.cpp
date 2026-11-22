#include "Core/Utf8.h"

void FUtf8::AppendCharacter(std::string& Text, char32_t Character)
{
	WW_CHECKF(IsCharacter(Character), "{:#x} is not a character", static_cast<uint32>(Character));

	const uint32 CodePoint = static_cast<uint32>(Character);
	if (CodePoint < 0x80)
	{
		Text.push_back(static_cast<char>(CodePoint));
	}
	else if (CodePoint < 0x800)
	{
		Text.push_back(static_cast<char>(0xC0 | (CodePoint >> 6)));
		Text.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
	}
	else if (CodePoint < 0x10000)
	{
		Text.push_back(static_cast<char>(0xE0 | (CodePoint >> 12)));
		Text.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
		Text.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
	}
	else
	{
		Text.push_back(static_cast<char>(0xF0 | (CodePoint >> 18)));
		Text.push_back(static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F)));
		Text.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
		Text.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
	}
}
