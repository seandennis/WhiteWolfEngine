#pragma once

#include "CoreMinimal.h"

#include <string>

/** Text in UTF-8, the encoding of every string in the engine. */
class FUtf8
{
public:
	/**
	 * Returns true if a value is a character that text can hold: a Unicode code point, at most U+10FFFF, that is
	 * not one of the values U+D800 to U+DFFF, which UTF-16 uses for the two halves of a surrogate pair.
	 */
	static constexpr bool IsCharacter(char32_t Value)
	{
		const bool bIsSurrogate = Value >= 0xD800 && Value <= 0xDFFF;
		return Value <= 0x10FFFF && !bIsSurrogate;
	}

	/**
	 * Appends a character to text, as the one to four bytes that encode it.
	 *
	 * @param Character A character for which IsCharacter is true. Anything else is a fatal error.
	 */
	ENGINE_API static void AppendCharacter(std::string& Text, char32_t Character);
};
