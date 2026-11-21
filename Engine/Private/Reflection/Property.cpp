#include "Reflection/Property.h"

#include <utility>

FProperty::FProperty(std::string InName, FValueAccessor InAccessor)
	: Name(std::move(InName))
	, Accessor(InAccessor)
{
}

FProperty::~FProperty() = default;

void* FProperty::GetValuePtr(void* Container) const
{
	WW_CHECKF(Accessor != nullptr, "A container element property has no location of its own");
	WW_CHECK(Container != nullptr);

	return Accessor(Container);
}

bool FProperty::ContainsObjectReferences() const
{
	return false;
}

void FProperty::VisitObjectReferences(void* /*Value*/, IObjectReferenceVisitor& /*Visitor*/) const
{
}
