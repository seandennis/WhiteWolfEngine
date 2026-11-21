#include "Reflection/WeakObjectProperty.h"

#include <utility>

FWeakObjectProperty::FWeakObjectProperty(std::string InName, FValueAccessor InAccessor, FClassGetter InGetPropertyClass)
	: FProperty(std::move(InName), InAccessor)
	, GetPropertyClassFunction(InGetPropertyClass)
{
	WW_CHECK(GetPropertyClassFunction != nullptr);
}

WClass* FWeakObjectProperty::GetPropertyClass() const
{
	return GetPropertyClassFunction();
}
