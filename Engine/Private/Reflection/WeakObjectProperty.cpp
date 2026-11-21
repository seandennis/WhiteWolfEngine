#include "Reflection/WeakObjectProperty.h"

#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/WeakObjectPtr.h"
#include "Reflection/ObjectReferenceJson.h"

#include <format>
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

std::string FWeakObjectProperty::GetTypeName() const
{
	return std::format("TWeakObjectPtr<{}>", GetPropertyClass()->GetName());
}

FJsonValue FWeakObjectProperty::ExportJson(const void* Value, IPropertyJsonContext& Context) const
{
	return FObjectReferenceJson::Export(static_cast<const FWeakObjectPtr*>(Value)->Get(), Context);
}

bool FWeakObjectProperty::ImportJson(
	void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const
{
	WObject* Object = nullptr;
	if (!FObjectReferenceJson::Import(Json, *GetPropertyClass(), Context, Object, OutError))
	{
		return false;
	}

	*static_cast<FWeakObjectPtr*>(Value) = FWeakObjectPtr(Object);
	return true;
}
