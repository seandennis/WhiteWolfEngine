#pragma once

#include "Core/CoreTypes.h"

#include <string>
#include <vector>

/** A member variable marked with WPROPERTY(). */
struct FReflectedProperty
{
	/** The member's name. */
	std::string Name;

	/** The names of the EPropertyFlags values its specifiers ask for, in the order written. */
	std::vector<std::string> Flags;

	/** The value of its Category specifier, without quotes; empty if it has none. */
	std::string Category;

	/** The line of the WPROPERTY(). */
	int32 Line = 0;

	bool operator==(const FReflectedProperty& Other) const = default;
};
