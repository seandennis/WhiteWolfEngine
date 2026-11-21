#pragma once

#include "CoreMinimal.h"

#include "Json/JsonFormat.h"

#include <string>

class FJsonValue;

/** Writes JSON text. */
class FJsonWriter
{
public:
	/**
	 * Returns Value as JSON text in UTF-8, without a trailing newline.
	 *
	 * The same value always produces the same text. Doubles are written with the fewest digits that read back as
	 * exactly the same double, and always with a decimal point or an exponent, so a double stays a double when the
	 * text is parsed again. JSON has no way to write infinity or NaN; such a number is written as null.
	 */
	ENGINE_API static std::string Write(const FJsonValue& Value, EJsonFormat Format = EJsonFormat::Pretty);
};
