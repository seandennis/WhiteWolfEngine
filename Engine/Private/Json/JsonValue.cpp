#include "Json/JsonValue.h"

#include <array>
#include <charconv>
#include <cmath>
#include <system_error>

FJsonValue::FJsonValue(float Value)
{
	if (!std::isfinite(Value))
	{
		Data = static_cast<double>(Value);
		return;
	}

	// The shortest decimal text that identifies this float, read back as a double.
	std::array<char, 32> Buffer{};
	const std::to_chars_result Written = std::to_chars(Buffer.data(), Buffer.data() + Buffer.size(), Value);
	WW_CHECK(Written.ec == std::errc());

	double DoubleValue = 0.0;
	const std::from_chars_result Read = std::from_chars(Buffer.data(), Written.ptr, DoubleValue);
	WW_CHECK(Read.ec == std::errc());

	Data = DoubleValue;
}

std::string_view FJsonValue::GetTypeDescription() const
{
	if (IsNull())
	{
		return "null";
	}
	if (IsBoolean())
	{
		return "a boolean";
	}
	if (IsNumber())
	{
		return "a number";
	}
	if (IsString())
	{
		return "a string";
	}
	if (IsArray())
	{
		return "an array";
	}

	return "an object";
}

bool FJsonValue::GetBoolean() const
{
	WW_CHECKF(IsBoolean(), "The JSON value is {}, not a boolean", GetTypeDescription());
	return std::get<bool>(Data);
}

int64 FJsonValue::GetInteger() const
{
	WW_CHECKF(IsInteger(), "The JSON value is {}, not an integer", GetTypeDescription());
	return std::get<int64>(Data);
}

uint64 FJsonValue::GetUnsignedInteger() const
{
	WW_CHECKF(IsUnsignedInteger(), "The JSON value is {}, not an unsigned integer", GetTypeDescription());
	return std::get<uint64>(Data);
}

double FJsonValue::GetNumber() const
{
	WW_CHECKF(IsNumber(), "The JSON value is {}, not a number", GetTypeDescription());

	if (IsInteger())
	{
		return static_cast<double>(std::get<int64>(Data));
	}
	if (IsUnsignedInteger())
	{
		return static_cast<double>(std::get<uint64>(Data));
	}

	return std::get<double>(Data);
}

const std::string& FJsonValue::GetString() const
{
	WW_CHECKF(IsString(), "The JSON value is {}, not a string", GetTypeDescription());
	return std::get<std::string>(Data);
}

const FJsonValue::FArray& FJsonValue::GetElements() const
{
	WW_CHECKF(IsArray(), "The JSON value is {}, not an array", GetTypeDescription());
	return std::get<FArray>(Data);
}

void FJsonValue::AddElement(FJsonValue Element)
{
	WW_CHECKF(IsArray(), "The JSON value is {}, not an array", GetTypeDescription());
	std::get<FArray>(Data).push_back(std::move(Element));
}

const FJsonValue::FObject& FJsonValue::GetMembers() const
{
	WW_CHECKF(IsObject(), "The JSON value is {}, not an object", GetTypeDescription());
	return std::get<FObject>(Data);
}

const FJsonValue* FJsonValue::FindMember(std::string_view Key) const
{
	for (const FMember& Member : GetMembers())
	{
		if (Member.first == Key)
		{
			return &Member.second;
		}
	}

	return nullptr;
}

void FJsonValue::SetMember(std::string Key, FJsonValue Value)
{
	WW_CHECKF(IsObject(), "The JSON value is {}, not an object", GetTypeDescription());

	FObject& Members = std::get<FObject>(Data);
	for (FMember& Member : Members)
	{
		if (Member.first == Key)
		{
			Member.second = std::move(Value);
			return;
		}
	}

	Members.emplace_back(std::move(Key), std::move(Value));
}
