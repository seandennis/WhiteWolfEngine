#include "Reflection/ArrayProperty.h"

#include <utility>

FArrayProperty::FArrayProperty(
	std::string InName, FValueAccessor InAccessor, std::unique_ptr<FProperty> InElementProperty)
	: FProperty(std::move(InName), InAccessor)
	, ElementProperty(std::move(InElementProperty))
{
	WW_CHECK(ElementProperty != nullptr);
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
