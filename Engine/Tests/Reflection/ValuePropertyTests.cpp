#include "Reflection/ValueProperty.h"

#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/Property.h"
#include "Reflection/TestReflectionObjects.h"

#include <doctest/doctest.h>

#include <limits>
#include <ostream>
#include <string>
#include <string_view>

namespace
{
	const FProperty& FindValueProperty(std::string_view Name)
	{
		const FProperty* const Property = WTestValueObject::StaticClass()->FindProperty(Name);
		REQUIRE(Property != nullptr);
		return *Property;
	}

	/** Exports the named property of Object. */
	FJsonValue Export(const WTestValueObject& Object, std::string_view Name)
	{
		FTestPropertyJsonContext Context;
		const FProperty& Property = FindValueProperty(Name);
		return Property.ExportJson(Property.GetValuePtr(&Object), Context);
	}

	/** Imports Json into the named property of Object and returns whether it worked. */
	bool Import(WTestValueObject& Object, std::string_view Name, const FJsonValue& Json, std::string& OutError)
	{
		FTestPropertyJsonContext Context;
		const FProperty& Property = FindValueProperty(Name);
		return Property.ImportJson(Property.GetValuePtr(&Object), Json, Context, OutError);
	}

	bool Import(WTestValueObject& Object, std::string_view Name, const FJsonValue& Json)
	{
		std::string Error;
		const bool bImported = Import(Object, Name, Json, Error);
		CHECK(bImported == Error.empty());
		return bImported;
	}

	FJsonValue MakeNumberArray(std::initializer_list<double> Numbers)
	{
		FJsonValue Array = FJsonValue::MakeArray();
		for (const double Number : Numbers)
		{
			Array.AddElement(Number);
		}
		return Array;
	}
}

TEST_SUITE("TValueProperty")
{
	TEST_CASE("Each value property reports the C++ type of its member")
	{
		CHECK(FindValueProperty("bFlag").GetTypeName() == "bool");
		CHECK(FindValueProperty("Int8Value").GetTypeName() == "int8");
		CHECK(FindValueProperty("Int16Value").GetTypeName() == "int16");
		CHECK(FindValueProperty("Int32Value").GetTypeName() == "int32");
		CHECK(FindValueProperty("Int64Value").GetTypeName() == "int64");
		CHECK(FindValueProperty("Uint8Value").GetTypeName() == "uint8");
		CHECK(FindValueProperty("Uint16Value").GetTypeName() == "uint16");
		CHECK(FindValueProperty("Uint32Value").GetTypeName() == "uint32");
		CHECK(FindValueProperty("Uint64Value").GetTypeName() == "uint64");
		CHECK(FindValueProperty("FloatValue").GetTypeName() == "float");
		CHECK(FindValueProperty("DoubleValue").GetTypeName() == "double");
		CHECK(FindValueProperty("Text").GetTypeName() == "std::string");
		CHECK(FindValueProperty("Location").GetTypeName() == "FVector");
		CHECK(FindValueProperty("Direction").GetTypeName() == "FVector3f");
		CHECK(FindValueProperty("Rotation").GetTypeName() == "FQuat");
		CHECK(FindValueProperty("Transform").GetTypeName() == "FTransform");
	}

	TEST_CASE("A value property holds no object references")
	{
		CHECK_FALSE(FindValueProperty("Int32Value").ContainsObjectReferences());
		CHECK_FALSE(FindValueProperty("Transform").ContainsObjectReferences());
	}

	TEST_CASE("A property without specifiers has no flags and no category")
	{
		const FProperty& Property = FindValueProperty("Int32Value");

		CHECK(Property.GetFlags() == EPropertyFlags::None);
		CHECK_FALSE(Property.HasAnyFlags(EPropertyFlags::Transient));
		CHECK(Property.GetCategory().empty());
	}

	TEST_CASE("A property keeps the flags and category it was registered with")
	{
		const FProperty& Property = FindValueProperty("TransientValue");

		CHECK(Property.HasAnyFlags(EPropertyFlags::Transient));
		CHECK(Property.HasAnyFlags(EPropertyFlags::BlueprintReadOnly));
		CHECK_FALSE(Property.HasAnyFlags(EPropertyFlags::BlueprintReadWrite));
		CHECK(Property.GetCategory() == "Debug");
	}

	TEST_CASE("A private member is reflected like a public one")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->SetPrivateValue(41);

		CHECK(Export(*Object, "PrivateValue") == FJsonValue(41));
		CHECK(Import(*Object, "PrivateValue", FJsonValue(42)));
		CHECK(Object->GetPrivateValue() == 42);
	}

	TEST_CASE("GetValuePtr works on a const object too")
	{
		const FScopedObjectTest ObjectTest;
		const WTestValueObject* const Object = NewObject<WTestValueObject>();

		CHECK(FindValueProperty("Int32Value").GetValuePtr(Object) == &Object->Int32Value);
	}

	TEST_CASE("Booleans, numbers and strings export as the matching JSON values")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->bFlag = true;
		Object->Int8Value = -8;
		Object->Int64Value = std::numeric_limits<int64>::min();
		Object->Uint32Value = 4000000000u;
		Object->FloatValue = 0.1f;
		Object->DoubleValue = 149597870700.5;
		Object->Text = "hello";

		CHECK(Export(*Object, "bFlag") == FJsonValue(true));
		CHECK(Export(*Object, "Int8Value") == FJsonValue(-8));
		CHECK(Export(*Object, "Int64Value") == FJsonValue(std::numeric_limits<int64>::min()));
		CHECK(Export(*Object, "Uint32Value") == FJsonValue(4000000000LL));
		CHECK(Export(*Object, "FloatValue") == FJsonValue(0.1));
		CHECK(Export(*Object, "DoubleValue") == FJsonValue(149597870700.5));
		CHECK(Export(*Object, "Text") == FJsonValue("hello"));
	}

	TEST_CASE("Math types export as arrays of numbers and a transform as an object of them")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Location = FVector(1.0e12, -2.5, 3.0);
		Object->Direction = FVector3f(0.5f, 0.25f, -1.0f);
		Object->Rotation = FQuat(0.5f, -0.5f, 0.5f, -0.5f);
		Object->Transform.Location = FVector(1.0, 2.0, 3.0);
		Object->Transform.Scale = FVector3f(2.0f, 2.0f, 2.0f);

		CHECK(Export(*Object, "Location") == MakeNumberArray({1.0e12, -2.5, 3.0}));
		CHECK(Export(*Object, "Direction") == MakeNumberArray({0.5, 0.25, -1.0}));
		// A quaternion is written in the order of FQuat's constructor: w, x, y, z.
		CHECK(Export(*Object, "Rotation") == MakeNumberArray({0.5, -0.5, 0.5, -0.5}));

		FJsonValue ExpectedTransform = FJsonValue::MakeObject();
		ExpectedTransform.SetMember("Location", MakeNumberArray({1.0, 2.0, 3.0}));
		ExpectedTransform.SetMember("Rotation", MakeNumberArray({1.0, 0.0, 0.0, 0.0}));
		ExpectedTransform.SetMember("Scale", MakeNumberArray({2.0, 2.0, 2.0}));
		CHECK(Export(*Object, "Transform") == ExpectedTransform);
	}

	TEST_CASE("Every value type imports what it exported")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Source = NewObject<WTestValueObject>();
		Source->bFlag = true;
		Source->Int8Value = std::numeric_limits<int8>::min();
		Source->Int16Value = std::numeric_limits<int16>::max();
		Source->Int32Value = std::numeric_limits<int32>::min();
		Source->Int64Value = std::numeric_limits<int64>::max();
		Source->Uint8Value = std::numeric_limits<uint8>::max();
		Source->Uint16Value = std::numeric_limits<uint16>::max();
		Source->Uint32Value = std::numeric_limits<uint32>::max();
		Source->Uint64Value = std::numeric_limits<uint64>::max();
		Source->FloatValue = 3.14159274f;
		Source->DoubleValue = 1.0e-300;
		Source->Text = "caf\xC3\xA9 \"quoted\"";
		Source->Location = FVector(1.0e13, -0.001, 42.0);
		Source->Direction = FVector3f(0.1f, 0.2f, 0.3f);
		Source->Rotation = FQuat(0.70710677f, 0.0f, 0.70710677f, 0.0f);
		Source->Transform.Location = FVector(-5.0, 6.0, 7.0e9);
		Source->Transform.Rotation = FQuat(0.0f, 1.0f, 0.0f, 0.0f);
		Source->Transform.Scale = FVector3f(1.0f, 2.0f, 3.0f);
		WTestValueObject* const Target = NewObject<WTestValueObject>();

		for (const char* const Name : {"bFlag",
				 "Int8Value",
				 "Int16Value",
				 "Int32Value",
				 "Int64Value",
				 "Uint8Value",
				 "Uint16Value",
				 "Uint32Value",
				 "Uint64Value",
				 "FloatValue",
				 "DoubleValue",
				 "Text",
				 "Location",
				 "Direction",
				 "Rotation",
				 "Transform"})
		{
			INFO("Property: ", Name);
			CHECK(Import(*Target, Name, Export(*Source, Name)));
		}

		CHECK(Target->bFlag == Source->bFlag);
		CHECK(Target->Int8Value == Source->Int8Value);
		CHECK(Target->Int16Value == Source->Int16Value);
		CHECK(Target->Int32Value == Source->Int32Value);
		CHECK(Target->Int64Value == Source->Int64Value);
		CHECK(Target->Uint8Value == Source->Uint8Value);
		CHECK(Target->Uint16Value == Source->Uint16Value);
		CHECK(Target->Uint32Value == Source->Uint32Value);
		CHECK(Target->Uint64Value == Source->Uint64Value);
		CHECK(Target->FloatValue == Source->FloatValue);
		CHECK(Target->DoubleValue == Source->DoubleValue);
		CHECK(Target->Text == Source->Text);
		CHECK(Target->Location == Source->Location);
		CHECK(Target->Direction == Source->Direction);
		CHECK(Target->Rotation == Source->Rotation);
		CHECK(Target->Transform == Source->Transform);
	}

	TEST_CASE("A floating-point property accepts an integer")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();

		CHECK(Import(*Object, "FloatValue", FJsonValue(3)));
		CHECK(Import(*Object, "DoubleValue", FJsonValue(-7)));

		CHECK(Object->FloatValue == 3.0f);
		CHECK(Object->DoubleValue == -7.0);
	}

	TEST_CASE("An integer property accepts a double that is a whole number in range")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();

		CHECK(Import(*Object, "Int32Value", FJsonValue(-12.0)));
		CHECK(Import(*Object, "Uint64Value", FJsonValue(9223372036854775808.0)));

		CHECK(Object->Int32Value == -12);
		CHECK(Object->Uint64Value == 9223372036854775808ull);
	}

	TEST_CASE("An integer property rejects a value outside its range and keeps its old value")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Int8Value = 5;
		Object->Uint8Value = 6;
		Object->Int32Value = 7;
		Object->Uint64Value = 8;

		CHECK_FALSE(Import(*Object, "Int8Value", FJsonValue(128)));
		CHECK_FALSE(Import(*Object, "Int8Value", FJsonValue(-129)));
		CHECK_FALSE(Import(*Object, "Uint8Value", FJsonValue(-1)));
		CHECK_FALSE(Import(*Object, "Uint8Value", FJsonValue(256)));
		CHECK_FALSE(Import(*Object, "Int32Value", FJsonValue(2147483648LL)));
		CHECK_FALSE(Import(*Object, "Int32Value", FJsonValue(1.5)));
		CHECK_FALSE(Import(*Object, "Int32Value", FJsonValue(1.0e30)));
		CHECK_FALSE(Import(*Object, "Uint64Value", FJsonValue(-1)));
		CHECK_FALSE(Import(*Object, "Uint64Value", FJsonValue(18446744073709551616.0)));
		CHECK_FALSE(Import(*Object, "Uint64Value", FJsonValue(std::numeric_limits<double>::quiet_NaN())));

		CHECK(Object->Int8Value == 5);
		CHECK(Object->Uint8Value == 6);
		CHECK(Object->Int32Value == 7);
		CHECK(Object->Uint64Value == 8);
	}

	TEST_CASE("A float property rejects a number too large for a float")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->FloatValue = 2.0f;

		CHECK_FALSE(Import(*Object, "FloatValue", FJsonValue(1.0e39)));
		CHECK(Object->FloatValue == 2.0f);

		CHECK(Import(*Object, "FloatValue", FJsonValue(static_cast<double>(std::numeric_limits<float>::max()))));
		CHECK(Object->FloatValue == std::numeric_limits<float>::max());
	}

	TEST_CASE("A property rejects JSON of another type and says what it expected")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		std::string Error;

		CHECK_FALSE(Import(*Object, "bFlag", FJsonValue(1), Error));
		CHECK(Error == "expected true or false but found a number");

		CHECK_FALSE(Import(*Object, "Int32Value", FJsonValue("12"), Error));
		CHECK(Error == "expected a number but found a string");

		CHECK_FALSE(Import(*Object, "FloatValue", FJsonValue(), Error));
		CHECK(Error == "expected a number but found null");

		CHECK_FALSE(Import(*Object, "Text", FJsonValue(12), Error));
		CHECK(Error == "expected a string but found a number");

		CHECK_FALSE(Import(*Object, "Transform", FJsonValue::MakeArray(), Error));
		CHECK(Error == "expected an object for FTransform but found an array");
	}

	TEST_CASE("A vector rejects an array of the wrong size or with a non-number, and keeps its old value")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Location = FVector(1.0, 2.0, 3.0);
		Object->Rotation = FQuat(0.0f, 1.0f, 0.0f, 0.0f);
		std::string Error;

		CHECK_FALSE(Import(*Object, "Location", MakeNumberArray({1.0, 2.0}), Error));
		CHECK(Error == "expected an array of 3 numbers for FVector");
		CHECK_FALSE(Import(*Object, "Location", FJsonValue(5.0), Error));
		CHECK_FALSE(Import(*Object, "Rotation", MakeNumberArray({1.0, 0.0, 0.0}), Error));
		CHECK(Error == "expected an array of 4 numbers for FQuat");

		FJsonValue WithString = MakeNumberArray({9.0, 9.0});
		WithString.AddElement("z");
		CHECK_FALSE(Import(*Object, "Location", WithString, Error));
		CHECK(Error == "component 2 of FVector: expected a number but found a string");

		CHECK(Object->Location == FVector(1.0, 2.0, 3.0));
		CHECK(Object->Rotation == FQuat(0.0f, 1.0f, 0.0f, 0.0f));
	}

	TEST_CASE("A transform member that is missing keeps its identity value")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Transform.Rotation = FQuat(0.0f, 0.0f, 1.0f, 0.0f);
		Object->Transform.Scale = FVector3f(4.0f);
		FJsonValue Json = FJsonValue::MakeObject();
		Json.SetMember("Location", MakeNumberArray({7.0, 8.0, 9.0}));

		CHECK(Import(*Object, "Transform", Json));

		CHECK(Object->Transform.Location == FVector(7.0, 8.0, 9.0));
		CHECK(Object->Transform.Rotation == FQuat(1.0f, 0.0f, 0.0f, 0.0f));
		CHECK(Object->Transform.Scale == FVector3f(1.0f));
	}

	TEST_CASE("A transform rejects an unknown member and a malformed one, and keeps its old value")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Transform.Location = FVector(1.0, 1.0, 1.0);
		std::string Error;

		FJsonValue Misspelled = FJsonValue::MakeObject();
		Misspelled.SetMember("Positon", MakeNumberArray({7.0, 8.0, 9.0}));
		CHECK_FALSE(Import(*Object, "Transform", Misspelled, Error));
		CHECK(Error == "FTransform has no member named \"Positon\"");

		FJsonValue Malformed = FJsonValue::MakeObject();
		Malformed.SetMember("Location", MakeNumberArray({7.0, 8.0, 9.0}));
		Malformed.SetMember("Scale", FJsonValue("big"));
		CHECK_FALSE(Import(*Object, "Transform", Malformed, Error));
		CHECK(Error == "expected an array of 3 numbers for FVector3f");

		CHECK(Object->Transform.Location == FVector(1.0, 1.0, 1.0));
	}
}
