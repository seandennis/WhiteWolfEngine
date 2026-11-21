#include "Reflection/ObjectProperty.h"

#include "Object/ObjectPtr.h"
#include "Reflection/ObjectReferenceVisitor.h"

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

bool FObjectProperty::ContainsObjectReferences() const
{
	return true;
}

void FObjectProperty::VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const
{
	// Every TObjectPtr<T> is an FObjectPtr and nothing more, whatever T is.
	Visitor.VisitObjectReference(*static_cast<FObjectPtr*>(Value));
}
