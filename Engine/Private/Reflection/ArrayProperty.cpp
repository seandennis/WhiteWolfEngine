#include "Reflection/ArrayProperty.h"

#include "Json/JsonValue.h"

#include <format>
#include <limits>
#include <utility>

FArrayProperty::FArrayProperty(
	std::string InName, FValueAccessor InAccessor, std::unique_ptr<FProperty> InElementProperty)
	: FProperty(std::move(InName), InAccessor)
	, ElementProperty(std::move(InElementProperty))
{
	WW_CHECK(ElementProperty != nullptr);
}

std::string FArrayProperty::GetTypeName() const
{
	return std::format("std::vector<{}>", ElementProperty->GetTypeName());
}

bool FArrayProperty::ContainsObjectReferences() const
{
	return ElementProperty->ContainsObjectReferences();
}

void FArrayProperty::VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const
{
	if (!ElementProperty->ContainsObjectReferences())
	{
		return;
	}

	const int32 Num = GetNum(Value);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		ElementProperty->VisitObjectReferences(GetElementPtr(Value, Index), Visitor);
	}
}

FJsonValue FArrayProperty::ExportJson(const void* Value, IPropertyJsonContext& Context) const
{
	FJsonValue Json = FJsonValue::MakeArray();

	const int32 Num = GetNum(Value);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Json.AddElement(ElementProperty->ExportJson(GetElementPtr(Value, Index), Context));
	}

	return Json;
}

bool FArrayProperty::ImportJson(
	void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const
{
	if (!Json.IsArray())
	{
		OutError = std::format("expected an array but found {}", Json.GetTypeDescription());
		return false;
	}

	const FJsonValue::FArray& Elements = Json.GetElements();
	if (Elements.size() > static_cast<std::size_t>(std::numeric_limits<int32>::max()))
	{
		OutError = std::format("an array of {} elements is too large", Elements.size());
		return false;
	}

	const int32 Num = static_cast<int32>(Elements.size());
	SetNum(Value, Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		std::string ElementError;
		const FJsonValue& Element = Elements[static_cast<std::size_t>(Index)];
		if (!ElementProperty->ImportJson(GetElementPtr(Value, Index), Element, Context, ElementError))
		{
			OutError = std::format("element {}: {}", Index, ElementError);
			return false;
		}
	}

	return true;
}
