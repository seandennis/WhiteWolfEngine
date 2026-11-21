#pragma once

#include "CoreMinimal.h"

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/**
 * A JSON value: null, a boolean, a number, a string, an array of values or an object.
 *
 * Numbers are kept as a 64-bit integer when they are written without a fraction or exponent and fit, and as a
 * double otherwise, so that large integers survive a round trip exactly. An object keeps its members in the order
 * they were added, which is the order they are written in; that keeps saved files stable and easy to compare.
 *
 * Strings are UTF-8. A default-constructed value is null.
 */
class FJsonValue
{
public:
	using FArray = std::vector<FJsonValue>;
	using FMember = std::pair<std::string, FJsonValue>;
	using FObject = std::vector<FMember>;

	FJsonValue() = default;

	FJsonValue(std::nullptr_t)
	{
	}

	/** A template so that only a real bool selects it, never a pointer or a number. */
	template <std::same_as<bool> BooleanType>
	FJsonValue(BooleanType bValue)
		: Data(bValue)
	{
	}

	/** An integer. A value too large for a signed 64-bit integer is stored as the nearest double. */
	template <std::integral IntegerType>
		requires(!std::same_as<IntegerType, bool>)
	FJsonValue(IntegerType Value)
	{
		if (std::in_range<int64>(Value))
		{
			Data = static_cast<int64>(Value);
		}
		else
		{
			Data = static_cast<double>(Value);
		}
	}

	FJsonValue(double Value)
		: Data(Value)
	{
	}

	/**
	 * A single-precision number. It is stored as the double that has the same shortest decimal form, so 0.1f is
	 * written as 0.1 rather than 0.10000000149011612, and converts back to exactly the same float.
	 */
	ENGINE_API FJsonValue(float Value);

	FJsonValue(std::string Value)
		: Data(std::move(Value))
	{
	}

	FJsonValue(std::string_view Value)
		: Data(std::string(Value))
	{
	}

	FJsonValue(const char* Value)
		: Data(std::string(Value))
	{
	}

	FJsonValue(FArray Elements)
		: Data(std::move(Elements))
	{
	}

	FJsonValue(FObject Members)
		: Data(std::move(Members))
	{
	}

	/** Returns an empty array. */
	static FJsonValue MakeArray()
	{
		return FJsonValue(FArray());
	}

	/** Returns an empty object. */
	static FJsonValue MakeObject()
	{
		return FJsonValue(FObject());
	}

	bool IsNull() const
	{
		return std::holds_alternative<std::monostate>(Data);
	}

	bool IsBoolean() const
	{
		return std::holds_alternative<bool>(Data);
	}

	/** Returns true for a number that is stored as an integer. */
	bool IsInteger() const
	{
		return std::holds_alternative<int64>(Data);
	}

	/** Returns true for any number, whether it is stored as an integer or as a double. */
	bool IsNumber() const
	{
		return IsInteger() || std::holds_alternative<double>(Data);
	}

	bool IsString() const
	{
		return std::holds_alternative<std::string>(Data);
	}

	bool IsArray() const
	{
		return std::holds_alternative<FArray>(Data);
	}

	bool IsObject() const
	{
		return std::holds_alternative<FObject>(Data);
	}

	/** Returns the kind of value in words, for error messages: "null", "boolean", "number", "string", "array",
	 * "object". */
	ENGINE_API std::string_view GetTypeName() const;

	// The accessors below are fatal errors when the value is of another type; test with the Is functions first.

	ENGINE_API bool GetBoolean() const;

	/** Returns the value of a number that is stored as an integer. */
	ENGINE_API int64 GetInteger() const;

	/** Returns the value of any number as a double. */
	ENGINE_API double GetNumber() const;

	ENGINE_API const std::string& GetString() const;

	/** Returns the elements of an array. */
	ENGINE_API const FArray& GetElements() const;

	/** Appends an element to an array. */
	ENGINE_API void AddElement(FJsonValue Element);

	/** Returns the members of an object, in the order they were added. */
	ENGINE_API const FObject& GetMembers() const;

	/** Returns the member of an object with the given key, or null if there is none. Keys are case-sensitive. */
	ENGINE_API const FJsonValue* FindMember(std::string_view Key) const;

	/** Sets a member of an object. A new key is added at the end; an existing key keeps its position. */
	ENGINE_API void SetMember(std::string Key, FJsonValue Value);

	/**
	 * Two values are equal if they have the same type and content. Object members must be in the same order, and
	 * an integer never equals a double, even when they represent the same number.
	 */
	bool operator==(const FJsonValue& Other) const = default;

private:
	std::variant<std::monostate, bool, int64, double, std::string, FArray, FObject> Data;
};
