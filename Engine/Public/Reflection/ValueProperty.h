#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyValueTraits.h"

#include <string>
#include <utility>

/**
 * A property whose value is a plain value type: a boolean, a number, a string or one of the math types. ValueType
 * must have a TPropertyValueTraits specialization.
 */
template <typename ValueType>
class TValueProperty final : public FProperty
{
public:
	TValueProperty(std::string InName, FValueAccessor InAccessor)
		: FProperty(std::move(InName), InAccessor)
	{
	}

	std::string GetTypeName() const override
	{
		return std::string(TPropertyValueTraits<ValueType>::TypeName);
	}

	FJsonValue ExportJson(const void* Value, IPropertyJsonContext& /*Context*/) const override
	{
		return TPropertyValueTraits<ValueType>::ToJson(*static_cast<const ValueType*>(Value));
	}

	bool ImportJson(
		void* Value, const FJsonValue& Json, IPropertyJsonContext& /*Context*/, std::string& OutError) const override
	{
		// Read into a temporary, so that a value that cannot be read leaves the property as it was.
		ValueType NewValue{};
		if (!TPropertyValueTraits<ValueType>::FromJson(Json, NewValue, OutError))
		{
			return false;
		}

		*static_cast<ValueType*>(Value) = std::move(NewValue);
		return true;
	}
};
