#include "Reflection/PropertyValueTraits.h"

#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <cstddef>

namespace
{
	/**
	 * How far the length of a quaternion that is read may be from one for the quaternion to be left exactly as it
	 * is. What the engine writes is far nearer than that and is read back to the last digit, and so is a rotation
	 * that someone wrote to four digits.
	 */
	constexpr float UnitLengthTolerance = 1.0e-4f;

	/**
	 * Reads a JSON array of exactly Count numbers into OutComponents.
	 *
	 * @param TypeName The type being read, for the error message.
	 */
	template <typename ComponentType, std::size_t Count>
	bool ReadComponents(const FJsonValue& Json,
		std::string_view TypeName,
		std::array<ComponentType, Count>& OutComponents,
		std::string& OutError)
	{
		if (!Json.IsArray() || Json.GetElements().size() != Count)
		{
			OutError = std::format("expected an array of {} numbers for {}", Count, TypeName);
			return false;
		}

		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			std::string ComponentError;
			const FJsonValue& Element = Json.GetElements()[Index];
			if (!TPropertyValueTraits<ComponentType>::FromJson(Element, OutComponents[Index], ComponentError))
			{
				OutError = std::format("component {} of {}: {}", Index, TypeName, ComponentError);
				return false;
			}
		}

		return true;
	}
}

FJsonValue TPropertyValueTraits<bool>::ToJson(const bool& bValue)
{
	return FJsonValue(bValue);
}

bool TPropertyValueTraits<bool>::FromJson(const FJsonValue& Json, bool& bOutValue, std::string& OutError)
{
	if (!Json.IsBoolean())
	{
		OutError = std::format("expected true or false but found {}", Json.GetTypeDescription());
		return false;
	}

	bOutValue = Json.GetBoolean();
	return true;
}

FJsonValue TPropertyValueTraits<std::string>::ToJson(const std::string& Value)
{
	return FJsonValue(Value);
}

bool TPropertyValueTraits<std::string>::FromJson(const FJsonValue& Json, std::string& OutValue, std::string& OutError)
{
	if (!Json.IsString())
	{
		OutError = std::format("expected a string but found {}", Json.GetTypeDescription());
		return false;
	}

	OutValue = Json.GetString();
	return true;
}

FJsonValue TPropertyValueTraits<FVector>::ToJson(const FVector& Value)
{
	return FJsonValue(FJsonValue::FArray{FJsonValue(Value.x), FJsonValue(Value.y), FJsonValue(Value.z)});
}

bool TPropertyValueTraits<FVector>::FromJson(const FJsonValue& Json, FVector& OutValue, std::string& OutError)
{
	std::array<double, 3> Components{};
	if (!ReadComponents(Json, TypeName, Components, OutError))
	{
		return false;
	}

	OutValue = FVector(Components[0], Components[1], Components[2]);
	return true;
}

FJsonValue TPropertyValueTraits<FVector3f>::ToJson(const FVector3f& Value)
{
	return FJsonValue(FJsonValue::FArray{FJsonValue(Value.x), FJsonValue(Value.y), FJsonValue(Value.z)});
}

bool TPropertyValueTraits<FVector3f>::FromJson(const FJsonValue& Json, FVector3f& OutValue, std::string& OutError)
{
	std::array<float, 3> Components{};
	if (!ReadComponents(Json, TypeName, Components, OutError))
	{
		return false;
	}

	OutValue = FVector3f(Components[0], Components[1], Components[2]);
	return true;
}

FJsonValue TPropertyValueTraits<FQuat>::ToJson(const FQuat& Value)
{
	return FJsonValue(
		FJsonValue::FArray{FJsonValue(Value.w), FJsonValue(Value.x), FJsonValue(Value.y), FJsonValue(Value.z)});
}

bool TPropertyValueTraits<FQuat>::FromJson(const FJsonValue& Json, FQuat& OutValue, std::string& OutError)
{
	std::array<float, 4> Components{};
	if (!ReadComponents(Json, TypeName, Components, OutError))
	{
		return false;
	}

	// A quaternion is a rotation here, and whatever turns something by one relies on its being of unit length. One
	// that is written longer or shorter, as a hand or an agent may write it, is taken for the rotation that it
	// turns as. One of no length turns as nothing in particular, and is no rotation.
	const FQuat Given(Components[0], Components[1], Components[2], Components[3]);
	const float Length = glm::length(Given);
	const bool bHasLength = std::isfinite(Length) && Length > 0.0f;
	if (!bHasLength)
	{
		OutError = std::format("expected a rotation for {} but found a quaternion without a length", TypeName);
		return false;
	}

	const bool bIsOfUnitLength = std::abs(Length - 1.0f) <= UnitLengthTolerance;
	OutValue = bIsOfUnitLength ? Given : Given / Length;
	return true;
}

FJsonValue TPropertyValueTraits<FLinearColor>::ToJson(const FLinearColor& Value)
{
	return FJsonValue(
		FJsonValue::FArray{FJsonValue(Value.R), FJsonValue(Value.G), FJsonValue(Value.B), FJsonValue(Value.A)});
}

bool TPropertyValueTraits<FLinearColor>::FromJson(const FJsonValue& Json, FLinearColor& OutValue, std::string& OutError)
{
	std::array<float, 4> Components{};
	if (!ReadComponents(Json, TypeName, Components, OutError))
	{
		return false;
	}

	OutValue = FLinearColor(Components[0], Components[1], Components[2], Components[3]);
	return true;
}

FJsonValue TPropertyValueTraits<FTransform>::ToJson(const FTransform& Value)
{
	FJsonValue Json = FJsonValue::MakeObject();
	Json.SetMember("Location", TPropertyValueTraits<FVector>::ToJson(Value.Location));
	Json.SetMember("Rotation", TPropertyValueTraits<FQuat>::ToJson(Value.Rotation));
	Json.SetMember("Scale", TPropertyValueTraits<FVector3f>::ToJson(Value.Scale));
	return Json;
}

bool TPropertyValueTraits<FTransform>::FromJson(const FJsonValue& Json, FTransform& OutValue, std::string& OutError)
{
	if (!Json.IsObject())
	{
		OutError = std::format("expected an object for FTransform but found {}", Json.GetTypeDescription());
		return false;
	}

	FTransform Transform;
	for (const FJsonValue::FMember& Member : Json.GetMembers())
	{
		bool bMemberRead = false;
		if (Member.first == "Location")
		{
			bMemberRead = TPropertyValueTraits<FVector>::FromJson(Member.second, Transform.Location, OutError);
		}
		else if (Member.first == "Rotation")
		{
			bMemberRead = TPropertyValueTraits<FQuat>::FromJson(Member.second, Transform.Rotation, OutError);
		}
		else if (Member.first == "Scale")
		{
			bMemberRead = TPropertyValueTraits<FVector3f>::FromJson(Member.second, Transform.Scale, OutError);
		}
		else
		{
			OutError = std::format("FTransform has no member named \"{}\"", Member.first);
		}

		if (!bMemberRead)
		{
			return false;
		}
	}

	OutValue = Transform;
	return true;
}
