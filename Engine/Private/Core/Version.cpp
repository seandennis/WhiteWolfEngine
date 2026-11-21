#include "Core/Version.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <format>

namespace
{
	/**
	 * Parses one version component: a non-negative decimal integer with no sign and no leading zeros.
	 *
	 * @return The value, or an empty optional if Text is not exactly one well-formed component.
	 */
	std::optional<int32> ParseVersionComponent(std::string_view Text)
	{
		if (Text.empty())
		{
			return std::nullopt;
		}

		const bool bHasLeadingZero = Text.size() > 1 && Text.front() == '0';
		if (bHasLeadingZero)
		{
			return std::nullopt;
		}

		// std::from_chars accepts a leading minus sign, which a version component must not have.
		const bool bStartsWithDigit = Text.front() >= '0' && Text.front() <= '9';
		if (!bStartsWithDigit)
		{
			return std::nullopt;
		}

		int32 Value = 0;
		const char* const End = Text.data() + Text.size();
		const std::from_chars_result Result = std::from_chars(Text.data(), End, Value);
		if (Result.ec != std::errc() || Result.ptr != End)
		{
			return std::nullopt;
		}

		return Value;
	}
}

std::optional<FVersion> FVersion::Parse(std::string_view Text)
{
	constexpr std::size_t ComponentCount = 3;
	std::array<int32, ComponentCount> Components = {};

	std::string_view Remaining = Text;
	for (std::size_t Index = 0; Index < ComponentCount; ++Index)
	{
		const bool bIsLastComponent = Index + 1 == ComponentCount;
		const std::size_t SeparatorPosition = Remaining.find('.');

		// Every component except the last must be followed by a dot, and the last must not be.
		if (bIsLastComponent != (SeparatorPosition == std::string_view::npos))
		{
			return std::nullopt;
		}

		const std::optional<int32> Component = ParseVersionComponent(Remaining.substr(0, SeparatorPosition));
		if (!Component.has_value())
		{
			return std::nullopt;
		}

		Components[Index] = *Component;
		if (!bIsLastComponent)
		{
			Remaining.remove_prefix(SeparatorPosition + 1);
		}
	}

	return FVersion{Components[0], Components[1], Components[2]};
}

std::string FVersion::ToString() const
{
	return std::format("{}.{}.{}", Major, Minor, Patch);
}
