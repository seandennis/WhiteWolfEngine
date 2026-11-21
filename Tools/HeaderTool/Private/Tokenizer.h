#pragma once

#include "Token.h"

#include <string_view>
#include <vector>

/** Splits C++ source text into tokens. */
class FTokenizer
{
public:
	/**
	 * Returns the tokens of Source in order.
	 *
	 * Comments and preprocessor directives produce no tokens, so a macro definition or a disabled block of code
	 * cannot be mistaken for a declaration. Text inside string and character literals is never split. A literal or
	 * comment that is not closed runs to the end of the text.
	 */
	static std::vector<FToken> Tokenize(std::string_view Source);
};
