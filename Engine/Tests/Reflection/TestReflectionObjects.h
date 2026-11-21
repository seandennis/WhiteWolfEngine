#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Reflection/PropertyJsonContext.h"

#include <string>
#include <vector>

/**
 * An object with a reflected member of every type that can be a property, and reflected functions of every shape.
 * The header tool generates its registration, so these tests cover the tool's output as well as the reflection
 * system itself.
 */
WCLASS()
class WTestValueObject : public WObject
{
	WW_DECLARE_CLASS(WTestValueObject, WObject)

public:
	WPROPERTY()
	bool bFlag = false;

	WPROPERTY()
	int8 Int8Value = 0;

	WPROPERTY()
	int16 Int16Value = 0;

	WPROPERTY()
	int32 Int32Value = 0;

	WPROPERTY()
	int64 Int64Value = 0;

	WPROPERTY()
	uint8 Uint8Value = 0;

	WPROPERTY()
	uint16 Uint16Value = 0;

	WPROPERTY()
	uint32 Uint32Value = 0;

	WPROPERTY()
	uint64 Uint64Value = 0;

	WPROPERTY()
	float FloatValue = 0.0f;

	WPROPERTY()
	double DoubleValue = 0.0;

	WPROPERTY()
	std::string Text;

	WPROPERTY()
	FVector Location = FVector(0.0);

	WPROPERTY()
	FVector3f Direction = FVector3f(0.0f);

	WPROPERTY()
	FQuat Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f);

	WPROPERTY()
	FTransform Transform;

	WPROPERTY()
	std::vector<int32> Numbers;

	WPROPERTY()
	std::vector<std::string> Names;

	WPROPERTY()
	TObjectPtr<WTestValueObject> Strong;

	WPROPERTY()
	TWeakObjectPtr<WTestValueObject> Weak;

	WPROPERTY()
	std::vector<TObjectPtr<WTestValueObject>> StrongArray;

	WPROPERTY(Transient, BlueprintReadOnly, Category = "Debug")
	float TransientValue = 0.0f;

	/** Not reflected. */
	int32 CallCount = 0;

	WFUNCTION(BlueprintPure, Category = "Math")
	int32 Add(int32 A, int32 B) const;

	WFUNCTION(BlueprintCallable)
	void SetText(const std::string& NewText);

	WFUNCTION()
	std::string GetTextCopy() const;

	WFUNCTION()
	const std::string& GetTextReference() const;

	WFUNCTION()
	void ScaleFloat(float Factor);

	WFUNCTION()
	bool TryDivide(int32 Numerator, int32 Denominator, int32& OutQuotient) const;

	WFUNCTION()
	WTestValueObject* GetStrong() const;

	WFUNCTION()
	void SetStrong(WTestValueObject* NewStrong);

	WFUNCTION()
	FVector Offset(const FVector& Delta) const;

	WFUNCTION()
	void CountCall();

	WFUNCTION()
	virtual std::string Describe() const;

	WFUNCTION()
	int32 GetPrivateValue() const
	{
		return PrivateValue;
	}

	void SetPrivateValue(int32 NewValue)
	{
		PrivateValue = NewValue;
	}

private:
	/** A private member can be reflected like any other. */
	WPROPERTY()
	int32 PrivateValue = 0;
};

/** Overrides a reflected virtual function. */
WCLASS()
class WTestDerivedValueObject : public WTestValueObject
{
	WW_DECLARE_CLASS(WTestDerivedValueObject, WTestValueObject)

public:
	std::string Describe() const override;
};

/**
 * Identifies objects by their position in a list, for tests that convert object references to and from JSON.
 * An object that is not in the list cannot be referred to.
 */
class FTestPropertyJsonContext final : public IPropertyJsonContext
{
public:
	/** The objects that can be referred to. An object's reference is its index here. */
	std::vector<WObject*> KnownObjects;

	FJsonValue ExportObjectReference(const WObject& Object) override;
	bool ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError) override;
};
