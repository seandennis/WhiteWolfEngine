#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

class FJsonValue;

/** Reads JSON text. */
class FJsonReader
{
public:
	/**
	 * Parses a complete JSON document.
	 *
	 * The text must be standard JSON (RFC 8259) in UTF-8: no comments, no trailing commas, and nothing after the
	 * value but whitespace. A leading byte order mark is accepted. An object with two members of the same key is
	 * rejected, as is a document nested more than 128 levels deep.
	 *
	 * @param Text The document.
	 * @param OutValue Receives the parsed value. Unchanged on failure.
	 * @param OutError Receives a message that starts with the line and column of the problem. Unchanged on success.
	 * @return True if the text was parsed.
	 */
	ENGINE_API static bool Parse(std::string_view Text, FJsonValue& OutValue, std::string& OutError);
};
