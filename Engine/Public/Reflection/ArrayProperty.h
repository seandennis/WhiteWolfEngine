#pragma once

#include "CoreMinimal.h"

#include "Reflection/Property.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

/**
 * A property whose value is a std::vector.
 *
 * This base class gives type-independent access to the array; TArrayProperty implements it for a concrete element
 * type. Each array property owns an element property that describes one element.
 */
class ENGINE_API FArrayProperty : public FProperty
{
public:
	/** Returns the property that describes one element. It has no name and no location of its own. */
	const FProperty& GetElementProperty() const
	{
		return *ElementProperty;
	}

	/** Returns the number of elements in the array at Value. */
	virtual int32 GetNum(const void* Value) const = 0;

	/** Returns the address of the element at Index in the array at Value. Index must be in range. */
	virtual void* GetElementPtr(void* Value, int32 Index) const = 0;

	bool ContainsObjectReferences() const override;
	void VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const override;

protected:
	FArrayProperty(std::string InName, FValueAccessor InAccessor, std::unique_ptr<FProperty> InElementProperty);

private:
	std::unique_ptr<FProperty> ElementProperty;
};

/** The array property for std::vector<ElementType>. */
template <typename ElementType>
class TArrayProperty final : public FArrayProperty
{
public:
	TArrayProperty(std::string InName, FValueAccessor InAccessor, std::unique_ptr<FProperty> InElementProperty)
		: FArrayProperty(std::move(InName), InAccessor, std::move(InElementProperty))
	{
	}

	int32 GetNum(const void* Value) const override
	{
		return static_cast<int32>(static_cast<const std::vector<ElementType>*>(Value)->size());
	}

	void* GetElementPtr(void* Value, int32 Index) const override
	{
		std::vector<ElementType>& Array = *static_cast<std::vector<ElementType>*>(Value);
		WW_CHECKF(Index >= 0 && static_cast<std::size_t>(Index) < Array.size(),
			"Index {} is out of range for an array of {} elements",
			Index,
			Array.size());
		return &Array[static_cast<std::size_t>(Index)];
	}
};
