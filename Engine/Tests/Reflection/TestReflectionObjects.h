#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Reflection/PropertyJsonContext.h"

#include <string>
#include <vector>

/** An object with a reflected member of every type that can be a property, and reflected functions of every shape. */
class WTestValueObject : public WObject
{
	WW_DECLARE_CLASS(WTestValueObject, WObject)

public:
	bool bFlag = false;
	int8 Int8Value = 0;
	int16 Int16Value = 0;
	int32 Int32Value = 0;
	int64 Int64Value = 0;
	uint8 Uint8Value = 0;
	uint16 Uint16Value = 0;
	uint32 Uint32Value = 0;
	uint64 Uint64Value = 0;
	float FloatValue = 0.0f;
	double DoubleValue = 0.0;
	std::string Text;
	FVector Location = FVector(0.0);
	FVector3f Direction = FVector3f(0.0f);
	FQuat Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f);
	FTransform Transform;
	std::vector<int32> Numbers;
	std::vector<std::string> Names;
	TObjectPtr<WTestValueObject> Strong;
	TWeakObjectPtr<WTestValueObject> Weak;
	std::vector<TObjectPtr<WTestValueObject>> StrongArray;

	/** Registered with the Transient flag and the category "Debug". */
	float TransientValue = 0.0f;

	/** Not reflected. */
	int32 CallCount = 0;

	int32 Add(int32 A, int32 B) const;
	void SetText(const std::string& NewText);
	std::string GetTextCopy() const;
	const std::string& GetTextReference() const;
	void ScaleFloat(float Factor);
	bool TryDivide(int32 Numerator, int32 Denominator, int32& OutQuotient) const;
	WTestValueObject* GetStrong() const;
	void SetStrong(WTestValueObject* NewStrong);
	FVector Offset(const FVector& Delta) const;
	void CountCall();
	virtual std::string Describe() const;
};

/** Overrides a reflected virtual function. */
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
