#include "Reflection/Property.h"

#include <utility>

FProperty::FProperty(std::string InName, FValueAccessor InAccessor)
	: Name(std::move(InName))
	, Accessor(InAccessor)
{
}

FProperty::~FProperty() = default;

void FProperty::SetSpecifiers(FPropertySpecifiers InSpecifiers)
{
	Specifiers = std::move(InSpecifiers);
}

void* FProperty::GetValuePtr(void* Container) const
{
	WW_CHECKF(Accessor != nullptr, "Only a property that describes a member variable has a location");
	WW_CHECK(Container != nullptr);

	return Accessor(Container);
}

const void* FProperty::GetValuePtr(const void* Container) const
{
	// The accessor only computes an address; it never writes through the pointer.
	return GetValuePtr(const_cast<void*>(Container));
}

bool FProperty::ContainsObjectReferences() const
{
	return false;
}

void FProperty::VisitObjectReferences(void* /*Value*/, IObjectReferenceVisitor& /*Visitor*/) const
{
}
