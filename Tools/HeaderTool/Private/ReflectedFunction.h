#pragma once

#include "Core/CoreTypes.h"

#include <string>
#include <vector>

/** A member function marked with WFUNCTION(). */
struct FReflectedFunction
{
	/** The function's name. */
	std::string Name;

	/** The name of each parameter, in order. */
	std::vector<std::string> ParameterNames;

	/** The names of the EFunctionFlags values its specifiers ask for, in the order written. */
	std::vector<std::string> Flags;

	/** The value of its Category specifier, without quotes; empty if it has none. */
	std::string Category;

	/** The line of the WFUNCTION(). */
	int32 Line = 0;

	bool operator==(const FReflectedFunction& Other) const = default;
};
