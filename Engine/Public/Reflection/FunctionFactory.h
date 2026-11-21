#pragma once

#include "CoreMinimal.h"

#include "Object/Object.h"
#include "Object/ObjectPtr.h"
#include "Reflection/Function.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyFactory.h"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * How a parameter or return value of type DeclaredType is handed over when a function is called through reflection.
 *
 * FStorage is the type the caller supplies. Values, const references and non-const references are supplied as the
 * plain value type; a non-const reference is an output parameter that the function may change.
 */
template <typename DeclaredType>
struct TFunctionValueTraits
{
	static_assert(!std::is_rvalue_reference_v<DeclaredType>, "A reflected function cannot take an rvalue reference");
	static_assert(!std::is_pointer_v<std::remove_cvref_t<DeclaredType>>,
		"A reflected function can take or return a pointer only to a WObject class");

	using FStorage = std::remove_cvref_t<DeclaredType>;

	static constexpr bool bIsOutput =
		std::is_lvalue_reference_v<DeclaredType> && !std::is_const_v<std::remove_reference_t<DeclaredType>>;

	/** Returns the argument to pass to the function for the value at Storage. */
	static FStorage& Get(void* Storage)
	{
		return *static_cast<FStorage*>(Storage);
	}

	/** Assigns a function's result to the value at Storage. */
	static void Store(void* Storage, const FStorage& Result)
	{
		*static_cast<FStorage*>(Storage) = Result;
	}
};

/** A pointer to an object is supplied as a TObjectPtr, the form in which every reflected reference is held. */
template <typename ObjectType>
	requires std::is_base_of_v<WObject, ObjectType>
struct TFunctionValueTraits<ObjectType*>
{
	using FStorage = TObjectPtr<std::remove_cv_t<ObjectType>>;

	static constexpr bool bIsOutput = false;

	static ObjectType* Get(void* Storage)
	{
		return static_cast<FStorage*>(Storage)->Get();
	}

	static void Store(void* Storage, ObjectType* Result)
	{
		// The function handed out a pointer to an object it may only read; the reference that is kept does not
		// promise that, exactly like every other TObjectPtr.
		*static_cast<FStorage*>(Storage) = FStorage(const_cast<std::remove_cv_t<ObjectType>*>(Result));
	}
};

/** Builds the reflection data of one member function from its type. Used through TMemberFunctionTraits. */
template <auto Function, bool bIsConstFunction, typename OwnerType, typename ReturnType, typename... ParameterTypes>
struct TMemberFunctionBinding
{
	static_assert(std::is_base_of_v<WObject, OwnerType>, "Reflected functions must belong to a WObject class");

	static constexpr std::size_t ParameterCount = sizeof...(ParameterTypes);
	static constexpr bool bIsConst = bIsConstFunction;

	/** The FFunction::FInvoker of the function. */
	static void Invoke(WObject* Target, void* const* Arguments, void* ReturnValue)
	{
		InvokeWithIndices(Target, Arguments, ReturnValue, std::index_sequence_for<ParameterTypes...>());
	}

	/** Creates one property per parameter. Names must hold ParameterCount names. */
	static std::vector<std::unique_ptr<FProperty>> MakeParameters(
		[[maybe_unused]] std::span<const std::string_view> Names)
	{
		std::vector<std::unique_ptr<FProperty>> Parameters;
		Parameters.reserve(ParameterCount);

		[[maybe_unused]] std::size_t Index = 0;
		(Parameters.push_back(MakeParameter<ParameterTypes>(Names[Index++])), ...);

		return Parameters;
	}

	/** Creates the property that describes the return value, or null for a function that returns void. */
	static std::unique_ptr<FProperty> MakeReturnProperty()
	{
		if constexpr (std::is_void_v<ReturnType>)
		{
			return nullptr;
		}
		else
		{
			using FStorage = typename TFunctionValueTraits<ReturnType>::FStorage;
			return TPropertyFactory<FStorage>::Create("ReturnValue", nullptr);
		}
	}

private:
	template <std::size_t... Indices>
	static void InvokeWithIndices(WObject* Target,
		[[maybe_unused]] void* const* Arguments,
		[[maybe_unused]] void* ReturnValue,
		std::index_sequence<Indices...>)
	{
		OwnerType* const Self = static_cast<OwnerType*>(Target);
		if constexpr (std::is_void_v<ReturnType>)
		{
			(Self->*Function)(TFunctionValueTraits<ParameterTypes>::Get(Arguments[Indices])...);
		}
		else
		{
			TFunctionValueTraits<ReturnType>::Store(
				ReturnValue, (Self->*Function)(TFunctionValueTraits<ParameterTypes>::Get(Arguments[Indices])...));
		}
	}

	template <typename ParameterType>
	static std::unique_ptr<FProperty> MakeParameter(std::string_view Name)
	{
		using FTraits = TFunctionValueTraits<ParameterType>;

		std::unique_ptr<FProperty> Parameter =
			TPropertyFactory<typename FTraits::FStorage>::Create(std::string(Name), nullptr);
		if (FTraits::bIsOutput)
		{
			Parameter->SetSpecifiers(FPropertySpecifiers{.Flags = EPropertyFlags::OutParameter, .Category = {}});
		}
		return Parameter;
	}
};

/** Breaks a pointer to a member function into its class, return type and parameter types. */
template <auto Function>
struct TMemberFunctionTraits;

template <typename OwnerType,
	typename ReturnType,
	typename... ParameterTypes,
	ReturnType (OwnerType::*Function)(ParameterTypes...)>
struct TMemberFunctionTraits<Function>
	: TMemberFunctionBinding<Function, false, OwnerType, ReturnType, ParameterTypes...>
{
};

template <typename OwnerType,
	typename ReturnType,
	typename... ParameterTypes,
	ReturnType (OwnerType::*Function)(ParameterTypes...) const>
struct TMemberFunctionTraits<Function>
	: TMemberFunctionBinding<Function, true, OwnerType, ReturnType, ParameterTypes...>
{
};

/**
 * Creates the description of a member function:
 *
 *     Class.AddFunction(MakeFunction<&WDoor::Open>("Open", {"Speed"}));
 *
 * The function must not be static or overloaded. Its parameter and return types must be types that can be reflected
 * properties, or pointers to WObject classes.
 *
 * @param ParameterNames The name of each parameter, in order. A different number of names is a fatal error.
 */
template <auto Function>
std::unique_ptr<FFunction> MakeFunction(
	std::string Name, std::initializer_list<std::string_view> ParameterNames = {}, FFunctionSpecifiers Specifiers = {})
{
	using FTraits = TMemberFunctionTraits<Function>;

	WW_CHECKF(ParameterNames.size() == FTraits::ParameterCount,
		"Function {} has {} parameters but {} parameter names were given",
		Name,
		FTraits::ParameterCount,
		ParameterNames.size());

	if (FTraits::bIsConst)
	{
		Specifiers.Flags |= EFunctionFlags::Const;
	}

	const std::span<const std::string_view> Names(ParameterNames.begin(), ParameterNames.size());
	return std::make_unique<FFunction>(std::move(Name),
		&FTraits::Invoke,
		FTraits::MakeParameters(Names),
		FTraits::MakeReturnProperty(),
		std::move(Specifiers));
}
