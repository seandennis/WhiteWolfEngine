#pragma once

#include "CoreMinimal.h"

#include <string_view>

/** The type of value that a parameter of an editor command takes. */
enum class ECommandArgumentType : uint8
{
	/** True or false. */
	Boolean,

	/** A whole number in the range of a signed 64-bit integer, such as the identifier of an entity. */
	Integer,

	/** Any number. */
	Number,

	/** Text, as UTF-8. */
	String,

	/** Any JSON value. For the value of a reflected property, whose type depends on the property. */
	Json,
};

/** Returns the name of a type as the list of commands shows it: "Boolean", "Integer", "Number", "String", "Json". */
EDITOR_API std::string_view GetCommandArgumentTypeName(ECommandArgumentType Type);
