#pragma once

#include "Core/CoreTypes.h"
#include "TokenType.h"

#include <string>

/** One token of a C++ source file. */
struct FToken
{
	ETokenType Type = ETokenType::Punctuation;

	/** The token's text exactly as it appears in the source. */
	std::string Text;

	/** The line the token starts on; the first line is 1. */
	int32 Line = 0;

	bool operator==(const FToken& Other) const = default;
};
