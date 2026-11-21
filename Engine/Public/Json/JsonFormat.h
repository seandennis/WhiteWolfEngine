#pragma once

#include "CoreMinimal.h"

/** How FJsonWriter lays out the text it produces. */
enum class EJsonFormat : uint8
{
	/**
	 * For files that people and agents read and compare: one object member per line, indented by two spaces. An
	 * array whose elements are all numbers, strings, booleans or null is kept on one line, so a vector reads as
	 * [1, 2, 3].
	 */
	Pretty,

	/** The shortest form: no whitespace at all. */
	Compact,
};
