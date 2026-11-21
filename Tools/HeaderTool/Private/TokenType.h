#pragma once

#include "Core/CoreTypes.h"

/** The kinds of token the header tool tells apart. */
enum class ETokenType : uint8
{
	/** A name or keyword: WCLASS, class, int32, Name. */
	Identifier,

	/** A numeric literal. */
	Number,

	/** A string literal, with its quotes and any prefix. */
	String,

	/** A character literal, with its quotes. */
	Character,

	/** An operator or punctuation mark. "::" is one token; every other mark is a single character. */
	Punctuation,
};
