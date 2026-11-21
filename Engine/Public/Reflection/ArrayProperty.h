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
 * type. Each array property owns an element property that describes one element. In JSON the value is an array of
 * the elements.
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

	/**
	 * Resizes the array at Value to Num elements, which must not be negative. Elements that are added are
	 * value-initialized; elements beyond Num are removed.
	 */
	virtual void SetNum(void* Value, int32 Num) const = 0;

	/** Returns the address of the element at Index in the array at Value. Index must be in range. */
	virtual void* GetElementPtr(void* Value, int32 Index) const = 0;
	virtual const void* GetElementPtr(const void* Value, int32 Index) const = 0;

	std::string GetTypeName() const override;
	bool ContainsObjectReferences() const override;
	void VisitObjectReferences(void* Value, IObjectReferenceVisitor& Visitor) const override;
	FJsonValue ExportJson(const void* Value, IPropertyJsonContext& Context) const override;
	bool ImportJson(
		void* Value, const FJsonValue& Json, IPropertyJsonContext& Context, std::string& OutError) const override;

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

	void SetNum(void* Value, int32 Num) const override
	{
		WW_CHECKF(Num >= 0, "An array cannot have {} elements", Num);
		static_cast<std::vector<ElementType>*>(Value)->resize(static_cast<std::size_t>(Num));
	}

	void* GetElementPtr(void* Value, int32 Index) const override
	{
		std::vector<ElementType>& Array = *static_cast<std::vector<ElementType>*>(Value);
		CheckIndex(Index, Array.size());
		return &Array[static_cast<std::size_t>(Index)];
	}

	const void* GetElementPtr(const void* Value, int32 Index) const override
	{
		const std::vector<ElementType>& Array = *static_cast<const std::vector<ElementType>*>(Value);
		CheckIndex(Index, Array.size());
		return &Array[static_cast<std::size_t>(Index)];
	}

private:
	static void CheckIndex(int32 Index, std::size_t Num)
	{
		WW_CHECKF(Index >= 0 && static_cast<std::size_t>(Index) < Num,
			"Index {} is out of range for an array of {} elements",
			Index,
			Num);
	}
};
