#include "Reflection/TestReflectionObjects.h"

#include "Reflection/FunctionFactory.h"
#include "Reflection/PropertyFactory.h"

#include <cstddef>
#include <format>

WW_IMPLEMENT_CLASS(WTestValueObject)

void WTestValueObject::RegisterClassMembers(WClass& Class)
{
	Class.AddProperty(MakeProperty<&WTestValueObject::bFlag>("bFlag"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Int8Value>("Int8Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Int16Value>("Int16Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Int32Value>("Int32Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Int64Value>("Int64Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Uint8Value>("Uint8Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Uint16Value>("Uint16Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Uint32Value>("Uint32Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Uint64Value>("Uint64Value"));
	Class.AddProperty(MakeProperty<&WTestValueObject::FloatValue>("FloatValue"));
	Class.AddProperty(MakeProperty<&WTestValueObject::DoubleValue>("DoubleValue"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Text>("Text"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Location>("Location"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Direction>("Direction"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Rotation>("Rotation"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Transform>("Transform"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Numbers>("Numbers"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Names>("Names"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Strong>("Strong"));
	Class.AddProperty(MakeProperty<&WTestValueObject::Weak>("Weak"));
	Class.AddProperty(MakeProperty<&WTestValueObject::StrongArray>("StrongArray"));
	Class.AddProperty(MakeProperty<&WTestValueObject::TransientValue>("TransientValue",
		FPropertySpecifiers{
			.Flags = EPropertyFlags::Transient | EPropertyFlags::BlueprintReadOnly,
			.Category = "Debug",
		}));

	Class.AddFunction(MakeFunction<&WTestValueObject::Add>(
		"Add", {"A", "B"}, FFunctionSpecifiers{.Flags = EFunctionFlags::BlueprintPure, .Category = "Math"}));
	Class.AddFunction(MakeFunction<&WTestValueObject::SetText>(
		"SetText", {"NewText"}, FFunctionSpecifiers{.Flags = EFunctionFlags::BlueprintCallable, .Category = {}}));
	Class.AddFunction(MakeFunction<&WTestValueObject::GetTextCopy>("GetTextCopy"));
	Class.AddFunction(MakeFunction<&WTestValueObject::GetTextReference>("GetTextReference"));
	Class.AddFunction(MakeFunction<&WTestValueObject::ScaleFloat>("ScaleFloat", {"Factor"}));
	Class.AddFunction(
		MakeFunction<&WTestValueObject::TryDivide>("TryDivide", {"Numerator", "Denominator", "OutQuotient"}));
	Class.AddFunction(MakeFunction<&WTestValueObject::GetStrong>("GetStrong"));
	Class.AddFunction(MakeFunction<&WTestValueObject::SetStrong>("SetStrong", {"NewStrong"}));
	Class.AddFunction(MakeFunction<&WTestValueObject::Offset>("Offset", {"Delta"}));
	Class.AddFunction(MakeFunction<&WTestValueObject::CountCall>("CountCall"));
	Class.AddFunction(MakeFunction<&WTestValueObject::Describe>("Describe"));
}

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

WW_IMPLEMENT_CLASS(WTestDerivedValueObject)

void WTestDerivedValueObject::RegisterClassMembers(WClass& /*Class*/)
{
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
