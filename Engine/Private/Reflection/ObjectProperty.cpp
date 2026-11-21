#include "Reflection/ObjectProperty.h"

#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectPtr.h"
#include "Reflection/ObjectReferenceJson.h"
#include "Reflection/ObjectReferenceVisitor.h"

#include <format>
#include <utility>

FObjectProperty::FObjectProperty(std::string InName, FValueAccessor InAccessor, FClassGetter InGetPropertyClass)
	: FProperty(std::move(InName), InAccessor)
	, GetPropertyClassFunction(InGetPropertyClass)
{
	WW_CHECK(GetPropertyClassFunction != nullptr);
}

WClass* FObjectProperty::GetPropertyClass() const
{
	return GetPropertyClassFunction();
}

std::string FObjectProperty::GetTypeName() const
{
	return std::format("TObjectPtr<{}>", GetPropertyClass()->GetName());
}

bool FObjectProperty::ContainsObjectReferences() const
{
	return true;
}

void FObjectProperty::VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const
{
	// Every TObjectPtr<T> is an FObjectPtr and nothing more, whatever T is.
	Visitor.VisitObjectReference(*static_cast<FObjectPtr*>(Value));
}

FJsonValue FObjectProperty::ExportJson(const void* Value, IPropertyJsonContext& Context) const
{
	return FObjectReferenceJson::Export(static_cast<const FObjectPtr*>(Value)->Get(), Context);
}

bool FObjectProperty::ImportJson(
	void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const
{
	WObject* Object = nullptr;
	if (!FObjectReferenceJson::Import(Json, *GetPropertyClass(), Context, Object, OutError))
	{
		return false;
	}

	*static_cast<FObjectPtr*>(Value) = FObjectPtr(Object);
	return true;
}
