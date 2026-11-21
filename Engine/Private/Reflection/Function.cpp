#include "Reflection/Function.h"

#include "Object/Class.h"
#include "Object/Object.h"

#include <utility>

FFunction::FFunction(std::string InName,
	FInvoker InInvoker,
	std::vector<std::unique_ptr<FProperty>> InParameters,
	std::unique_ptr<FProperty> InReturnProperty,
	FFunctionSpecifiers InSpecifiers)
	: Name(std::move(InName))
	, Invoker(InInvoker)
	, Parameters(std::move(InParameters))
	, ReturnProperty(std::move(InReturnProperty))
	, Specifiers(std::move(InSpecifiers))
{
	WW_CHECK(Invoker != nullptr);
	for (const std::unique_ptr<FProperty>& Parameter : Parameters)
	{
		WW_CHECKF(Parameter != nullptr, "Function {} has a parameter without a property", Name);
	}
}

FFunction::~FFunction() = default;

void FFunction::Invoke(WObject* Target, std::span<void* const> Arguments, void* ReturnValue) const
{
	WW_CHECKF(OwnerClass != nullptr, "Function {} does not belong to a class", Name);
	WW_CHECKF(Target != nullptr, "Function {}::{} was called without an object", OwnerClass->GetName(), Name);
	WW_CHECKF(Target->IsA(OwnerClass),
		"Function {}::{} was called on a {}",
		OwnerClass->GetName(),
		Name,
		Target->GetClass()->GetName());
	WW_CHECKF(Arguments.size() == Parameters.size(),
		"Function {}::{} takes {} arguments but was called with {}",
		OwnerClass->GetName(),
		Name,
		Parameters.size(),
		Arguments.size());
	for (void* const Argument : Arguments)
	{
		WW_CHECKF(Argument != nullptr, "Function {}::{} was called with a null argument", OwnerClass->GetName(), Name);
	}

	const bool bReturnsValue = ReturnProperty != nullptr;
	const bool bHasReturnStorage = ReturnValue != nullptr;
	WW_CHECKF(bReturnsValue == bHasReturnStorage,
		"Function {}::{} {} a value but the caller {} a place for one",
		OwnerClass->GetName(),
		Name,
		bReturnsValue ? "returns" : "does not return",
		bHasReturnStorage ? "passed" : "did not pass");

	Invoker(Target, Arguments.data(), ReturnValue);
}
