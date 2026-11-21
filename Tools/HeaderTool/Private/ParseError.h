#pragma once

#include "Core/CoreTypes.h"

#include <string>

/** Something in a header that the header tool cannot accept. */
struct FParseError
{
	/** The line the problem is on; the first line is 1. */
	int32 Line = 0;

	/** What is wrong and, where it helps, what to write instead. */
	std::string Message;

	bool operator==(const FParseError& Other) const = default;
};
