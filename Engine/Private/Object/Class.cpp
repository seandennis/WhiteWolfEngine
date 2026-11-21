#include "Object/Class.h"

#include "Object/ObjectSystem.h"

#include <utility>

WClass::WClass() = default;

WClass* WClass::StaticClass()
{
	return FObjectSystem::Get().GetClassClass();
}

WClass* WClass::RegisterNativeClass(const FNativeClassDescription& Description)
{
	return FObjectSystem::Get().RegisterNativeClass(Description);
}

WClass* WClass::FindByName(std::string_view ClassName)
{
	return FObjectSystem::Get().FindClass(ClassName);
}

bool WClass::IsChildOf(const WClass* Other) const
{
	for (const WClass* Current = this; Current != nullptr; Current = Current->SuperClass)
	{
		if (Current == Other)
		{
			return true;
		}
	}

	return false;
}

void WClass::AddProperty(std::unique_ptr<FProperty> Property)
{
	WW_CHECK(Property != nullptr);
	WW_CHECKF(bIsRegistering,
		"Property '{}' was added to class {} after the class was registered",
		Property->GetName(),
		Name);
	WW_CHECKF(!Property->GetName().empty(), "A property of class {} has no name", Name);
	WW_CHECKF(FindProperty(Property->GetName()) == nullptr,
		"Class {} already has a property named '{}'",
		Name,
		Property->GetName());

	OwnProperties.push_back(std::move(Property));
}

void WClass::AddFunction(std::unique_ptr<FFunction> Function)
{
	WW_CHECK(Function != nullptr);
	WW_CHECKF(bIsRegistering,
		"Function '{}' was added to class {} after the class was registered",
		Function->GetName(),
		Name);
	WW_CHECKF(!Function->GetName().empty(), "A function of class {} has no name", Name);
	WW_CHECKF(FindFunction(Function->GetName()) == nullptr,
		"Class {} already has a function named '{}'",
		Name,
		Function->GetName());
	WW_CHECKF(Function->OwnerClass == nullptr,
		"Function '{}' already belongs to class {}",
		Function->GetName(),
		Function->OwnerClass->GetName());

	Function->OwnerClass = this;
	OwnFunctions.push_back(std::move(Function));
}

const FFunction* WClass::FindFunction(std::string_view FunctionName) const
{
	for (const WClass* Current = this; Current != nullptr; Current = Current->SuperClass)
	{
		for (const std::unique_ptr<FFunction>& Function : Current->OwnFunctions)
		{
			if (Function->GetName() == FunctionName)
			{
				return Function.get();
			}
		}
	}

	return nullptr;
}

const FProperty* WClass::FindProperty(std::string_view PropertyName) const
{
	for (const WClass* Current = this; Current != nullptr; Current = Current->SuperClass)
	{
		for (const std::unique_ptr<FProperty>& Property : Current->OwnProperties)
		{
			if (Property->GetName() == PropertyName)
			{
				return Property.get();
			}
		}
	}

	return nullptr;
}
