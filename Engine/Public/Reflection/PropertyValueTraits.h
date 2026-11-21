#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"

#include <cmath>
#include <concepts>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

/**
 * Describes a value type that can be a reflected property: its name and how it converts to and from JSON.
 *
 * There is a specialization for each such type: bool, the fixed-width integers, float, double, std::string, FVector,
 * FVector3f, FQuat and FTransform. Each provides:
 *
 *     static constexpr std::string_view TypeName;
 *     static FJsonValue ToJson(const ValueType& Value);
 *     static bool FromJson(const FJsonValue& Json, ValueType& OutValue, std::string& OutError);
 *
 * FromJson returns false and explains why in OutError when Json does not hold a value of the type; OutValue is then
 * in an unspecified but valid state.
 */
template <typename ValueType>
struct TPropertyValueTraits;

template <>
struct TPropertyValueTraits<bool>
{
	static constexpr std::string_view TypeName = "bool";

	ENGINE_API static FJsonValue ToJson(const bool& bValue);
	ENGINE_API static bool FromJson(const FJsonValue& Json, bool& bOutValue, std::string& OutError);
};

/** True for the number types that can be reflected: the fixed-width integers, float and double. */
template <typename Type>
inline constexpr bool TIsReflectedNumber = std::same_as<Type, int8> || std::same_as<Type, int16> ||
	std::same_as<Type, int32> || std::same_as<Type, int64> || std::same_as<Type, uint8> || std::same_as<Type, uint16> ||
	std::same_as<Type, uint32> || std::same_as<Type, uint64> || std::same_as<Type, float> || std::same_as<Type, double>;

/** Returns the engine's name for a reflected number type: "int32", "float". */
template <typename NumberType>
	requires TIsReflectedNumber<NumberType>
constexpr std::string_view GetReflectedNumberTypeName()
{
	if constexpr (std::same_as<NumberType, int8>)
	{
		return "int8";
	}
	else if constexpr (std::same_as<NumberType, int16>)
	{
		return "int16";
	}
	else if constexpr (std::same_as<NumberType, int32>)
	{
		return "int32";
	}
	else if constexpr (std::same_as<NumberType, int64>)
	{
		return "int64";
	}
	else if constexpr (std::same_as<NumberType, uint8>)
	{
		return "uint8";
	}
	else if constexpr (std::same_as<NumberType, uint16>)
	{
		return "uint16";
	}
	else if constexpr (std::same_as<NumberType, uint32>)
	{
		return "uint32";
	}
	else if constexpr (std::same_as<NumberType, uint64>)
	{
		return "uint64";
	}
	else if constexpr (std::same_as<NumberType, float>)
	{
		return "float";
	}
	else
	{
		return "double";
	}
}

template <typename NumberType>
	requires TIsReflectedNumber<NumberType>
struct TPropertyValueTraits<NumberType>
{
	static constexpr std::string_view TypeName = GetReflectedNumberTypeName<NumberType>();

	static FJsonValue ToJson(const NumberType& Value)
	{
		return FJsonValue(Value);
	}

	static bool FromJson(const FJsonValue& Json, NumberType& OutValue, std::string& OutError)
	{
		if (!Json.IsNumber())
		{
			OutError = std::format("expected a number but found {}", Json.GetTypeDescription());
			return false;
		}

		if constexpr (std::floating_point<NumberType>)
		{
			const double Value = Json.GetNumber();
			const bool bFits =
				!std::isfinite(Value) || std::abs(Value) <= static_cast<double>(std::numeric_limits<NumberType>::max());
			if (!bFits)
			{
				OutError = std::format("{} is out of range for {}", Value, TypeName);
				return false;
			}

			OutValue = static_cast<NumberType>(Value);
			return true;
		}
		else
		{
			if (Json.IsInteger())
			{
				const int64 Value = Json.GetInteger();
				if (!std::in_range<NumberType>(Value))
				{
					OutError = std::format("{} is out of range for {}", Value, TypeName);
					return false;
				}

				OutValue = static_cast<NumberType>(Value);
				return true;
			}

			if (Json.IsUnsignedInteger())
			{
				const uint64 Value = Json.GetUnsignedInteger();
				if (!std::in_range<NumberType>(Value))
				{
					OutError = std::format("{} is out of range for {}", Value, TypeName);
					return false;
				}

				OutValue = static_cast<NumberType>(Value);
				return true;
			}

			// A double is accepted when it holds a whole number in range, so that 3.0 can be read as 3.
			const double Value = Json.GetNumber();
			const double LowerBound = static_cast<double>(std::numeric_limits<NumberType>::min());
			const double ExclusiveUpperBound = std::ldexp(1.0, std::numeric_limits<NumberType>::digits);
			const bool bIsWholeNumber = std::isfinite(Value) && std::trunc(Value) == Value;
			if (!bIsWholeNumber || Value < LowerBound || Value >= ExclusiveUpperBound)
			{
				OutError = std::format("{} is not a whole number in the range of {}", Value, TypeName);
				return false;
			}

			OutValue = static_cast<NumberType>(Value);
			return true;
		}
	}
};

template <>
struct TPropertyValueTraits<std::string>
{
	static constexpr std::string_view TypeName = "std::string";

	ENGINE_API static FJsonValue ToJson(const std::string& Value);
	ENGINE_API static bool FromJson(const FJsonValue& Json, std::string& OutValue, std::string& OutError);
};

/** A double-precision vector is saved as [x, y, z]. */
template <>
struct TPropertyValueTraits<FVector>
{
	static constexpr std::string_view TypeName = "FVector";

	ENGINE_API static FJsonValue ToJson(const FVector& Value);
	ENGINE_API static bool FromJson(const FJsonValue& Json, FVector& OutValue, std::string& OutError);
};

/** A single-precision vector is saved as [x, y, z]. */
template <>
struct TPropertyValueTraits<FVector3f>
{
	static constexpr std::string_view TypeName = "FVector3f";

	ENGINE_API static FJsonValue ToJson(const FVector3f& Value);
	ENGINE_API static bool FromJson(const FJsonValue& Json, FVector3f& OutValue, std::string& OutError);
};

/** A quaternion is saved as [w, x, y, z], the order of FQuat's constructor. */
template <>
struct TPropertyValueTraits<FQuat>
{
	static constexpr std::string_view TypeName = "FQuat";

	ENGINE_API static FJsonValue ToJson(const FQuat& Value);
	ENGINE_API static bool FromJson(const FJsonValue& Json, FQuat& OutValue, std::string& OutError);
};

/**
 * A transform is saved as an object with the members "Location", "Rotation" and "Scale". A member that is missing
 * when a transform is loaded keeps its identity value; a member with any other name is an error.
 */
template <>
struct TPropertyValueTraits<FTransform>
{
	static constexpr std::string_view TypeName = "FTransform";

	ENGINE_API static FJsonValue ToJson(const FTransform& Value);
	ENGINE_API static bool FromJson(const FJsonValue& Json, FTransform& OutValue, std::string& OutError);
};
