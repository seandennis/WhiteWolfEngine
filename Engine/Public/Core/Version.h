#pragma once

#include "CoreMinimal.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>

/**
 * A version number of the form Major.Minor.Patch.
 *
 * Versions order by Major, then Minor, then Patch.
 */
struct FVersion
{
	int32 Major = 0;
	int32 Minor = 0;
	int32 Patch = 0;

	/**
	 * Parses a version written as three non-negative decimal integers separated by dots, for example "1.4.0".
	 *
	 * Parsing is strict: signs, whitespace, leading zeros, missing or extra components, and values that do not fit
	 * in an int32 are all rejected.
	 *
	 * @param Text The text to parse.
	 * @return The parsed version, or an empty optional if Text is not a well-formed version.
	 */
	ENGINE_API static std::optional<FVersion> Parse(std::string_view Text);

	/** Returns the version formatted as "Major.Minor.Patch". */
	ENGINE_API std::string ToString() const;

	constexpr auto operator<=>(const FVersion& Other) const = default;
};
