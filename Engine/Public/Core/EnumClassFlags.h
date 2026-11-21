#pragma once

#include "Core/CoreTypes.h"

#include <type_traits>

/**
 * Defines the bitwise operators |, &, ~, |= and &= for an enum class whose values are bit flags:
 *
 *     enum class EPropertyFlags : uint8 { None = 0, Transient = 1 << 0, BlueprintReadOnly = 1 << 1 };
 *     WW_ENUM_CLASS_FLAGS(EPropertyFlags)
 *
 * Use it at namespace scope, directly after the enum.
 */
#define WW_ENUM_CLASS_FLAGS(EnumType) \
	constexpr EnumType operator|(EnumType Left, EnumType Right) \
	{ \
		using FUnderlying = std::underlying_type_t<EnumType>; \
		return static_cast<EnumType>(static_cast<FUnderlying>(Left) | static_cast<FUnderlying>(Right)); \
	} \
	constexpr EnumType operator&(EnumType Left, EnumType Right) \
	{ \
		using FUnderlying = std::underlying_type_t<EnumType>; \
		return static_cast<EnumType>(static_cast<FUnderlying>(Left) & static_cast<FUnderlying>(Right)); \
	} \
	constexpr EnumType operator~(EnumType Value) \
	{ \
		using FUnderlying = std::underlying_type_t<EnumType>; \
		return static_cast<EnumType>(~static_cast<FUnderlying>(Value)); \
	} \
	constexpr EnumType& operator|=(EnumType& Left, EnumType Right) \
	{ \
		Left = Left | Right; \
		return Left; \
	} \
	constexpr EnumType& operator&=(EnumType& Left, EnumType Right) \
	{ \
		Left = Left & Right; \
		return Left; \
	}

/** Returns true if Value has at least one of the bits in Flags set. */
template <typename EnumType>
	requires std::is_enum_v<EnumType>
constexpr bool EnumHasAnyFlags(EnumType Value, EnumType Flags)
{
	using FUnderlying = std::underlying_type_t<EnumType>;
	return (static_cast<FUnderlying>(Value) & static_cast<FUnderlying>(Flags)) != 0;
}

/** Returns true if Value has every bit in Flags set. */
template <typename EnumType>
	requires std::is_enum_v<EnumType>
constexpr bool EnumHasAllFlags(EnumType Value, EnumType Flags)
{
	using FUnderlying = std::underlying_type_t<EnumType>;
	return (static_cast<FUnderlying>(Value) & static_cast<FUnderlying>(Flags)) == static_cast<FUnderlying>(Flags);
}
