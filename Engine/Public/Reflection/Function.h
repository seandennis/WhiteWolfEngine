#pragma once

#include "CoreMinimal.h"

#include "Core/EnumClassFlags.h"
#include "Reflection/FunctionFlags.h"
#include "Reflection/FunctionSpecifiers.h"
#include "Reflection/Property.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

class WClass;
class WObject;

/**
 * Describes one reflected member function of a class, a WFUNCTION(), and can call it on an object without knowing
 * its C++ signature.
 *
 * Each parameter and the return value are described by a property, which gives the type that a caller must supply:
 * the parameter's own type without const or reference, except that a pointer to a WObject class is supplied as a
 * TObjectPtr. A parameter that is a non-const reference has the flag EPropertyFlags::OutParameter.
 */
class ENGINE_API FFunction
{
public:
	/**
	 * Calls the function on Target. Arguments holds one value pointer per parameter and ReturnValue is the value
	 * pointer for the result, each pointing to a value of the type its property describes.
	 */
	using FInvoker = void (*)(WObject* Target, void* const* Arguments, void* ReturnValue);

	/**
	 * @param InParameters One property per parameter, in declaration order, named after the parameter.
	 * @param InReturnProperty Describes the return value; null if the function returns void.
	 */
	FFunction(std::string InName,
		FInvoker InInvoker,
		std::vector<std::unique_ptr<FProperty>> InParameters,
		std::unique_ptr<FProperty> InReturnProperty,
		FFunctionSpecifiers InSpecifiers);

	FFunction(const FFunction&) = delete;
	FFunction& operator=(const FFunction&) = delete;
	~FFunction();

	const std::string& GetName() const
	{
		return Name;
	}

	/** Returns the class that declares the function, or null if it has not been added to a class yet. */
	const WClass* GetOwnerClass() const
	{
		return OwnerClass;
	}

	/** Returns one property per parameter, in declaration order. */
	const std::vector<std::unique_ptr<FProperty>>& GetParameters() const
	{
		return Parameters;
	}

	/** Returns the property that describes the return value, or null if the function returns void. */
	const FProperty* GetReturnProperty() const
	{
		return ReturnProperty.get();
	}

	EFunctionFlags GetFlags() const
	{
		return Specifiers.Flags;
	}

	/** Returns true if the function has at least one of the given flags. */
	bool HasAnyFlags(EFunctionFlags Flags) const
	{
		return EnumHasAnyFlags(Specifiers.Flags, Flags);
	}

	/** Returns the group the function is listed under, or an empty string for the default group. */
	const std::string& GetCategory() const
	{
		return Specifiers.Category;
	}

	/**
	 * Calls the function.
	 *
	 * @param Target The object to call it on: an instance of the owner class or of a class derived from it.
	 * @param Arguments One pointer per parameter, each to a value of the type the parameter's property describes.
	 * The function may change a value whose parameter is an OutParameter.
	 * @param ReturnValue Where the result is assigned: a pointer to an existing value of the return property's
	 * type. Must be null if and only if the function returns void.
	 *
	 * Passing the wrong target, number of arguments or return pointer is a fatal error. The types of the values
	 * cannot be checked; passing a pointer to a value of another type is undefined behaviour.
	 */
	void Invoke(WObject* Target, std::span<void* const> Arguments, void* ReturnValue) const;

private:
	friend class WClass;

	std::string Name;
	FInvoker Invoker;
	std::vector<std::unique_ptr<FProperty>> Parameters;
	std::unique_ptr<FProperty> ReturnProperty;
	FFunctionSpecifiers Specifiers;

	/** Set by WClass::AddFunction. */
	const WClass* OwnerClass = nullptr;
};
