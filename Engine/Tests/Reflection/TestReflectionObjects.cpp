#include "Reflection/TestReflectionObjects.h"

#include <cstddef>
#include <format>

int32 WTestValueObject::Add(int32 A, int32 B) const
{
	return A + B;
}

void WTestValueObject::SetText(const std::string& NewText)
{
	Text = NewText;
}

std::string WTestValueObject::GetTextCopy() const
{
	return Text;
}

const std::string& WTestValueObject::GetTextReference() const
{
	return Text;
}

void WTestValueObject::ScaleFloat(float Factor)
{
	FloatValue *= Factor;
}

bool WTestValueObject::TryDivide(int32 Numerator, int32 Denominator, int32& OutQuotient) const
{
	if (Denominator == 0)
	{
		return false;
	}

	OutQuotient = Numerator / Denominator;
	return true;
}

WTestValueObject* WTestValueObject::GetStrong() const
{
	return Strong;
}

void WTestValueObject::SetStrong(WTestValueObject* NewStrong)
{
	Strong = NewStrong;
}

FVector WTestValueObject::Offset(const FVector& Delta) const
{
	return Location + Delta;
}

void WTestValueObject::CountCall()
{
	++CallCount;
}

std::string WTestValueObject::Describe() const
{
	return "base";
}

std::string WTestDerivedValueObject::Describe() const
{
	return "derived";
}

FJsonValue FTestPropertyJsonContext::ExportObjectReference(const WObject& Object)
{
	for (std::size_t Index = 0; Index < KnownObjects.size(); ++Index)
	{
		if (KnownObjects[Index] == &Object)
		{
			return FJsonValue(Index);
		}
	}

	return FJsonValue();
}

bool FTestPropertyJsonContext::ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError)
{
	const bool bIsKnownIndex =
		Json.IsInteger() && Json.GetInteger() >= 0 && static_cast<std::size_t>(Json.GetInteger()) < KnownObjects.size();
	if (!bIsKnownIndex)
	{
		OutError = std::format("there is no object {}", Json.IsInteger() ? Json.GetInteger() : -1);
		return false;
	}

	OutObject = KnownObjects[static_cast<std::size_t>(Json.GetInteger())];
	return true;
}
