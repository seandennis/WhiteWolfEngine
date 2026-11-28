#include "Reflection/ObjectProperties.h"

#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/Property.h"
#include "Reflection/TestReflectionObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Returns the value of JSON text, which the test expects to be valid. */
	FJsonValue ParseJson(std::string_view Text)
	{
		FJsonValue Value;
		std::string Error;
		INFO("Error: ", Error);
		REQUIRE(FJsonReader::Parse(Text, Value, Error));
		return Value;
	}

	/** Returns the names of the members of a JSON object, in their order. */
	std::vector<std::string> GetMemberNames(const FJsonValue& Object)
	{
		std::vector<std::string> Names;
		for (const FJsonValue::FMember& Member : Object.GetMembers())
		{
			Names.push_back(Member.first);
		}
		return Names;
	}

	/** Returns the names of properties, in their order. */
	std::vector<std::string> GetNames(const std::vector<const FProperty*>& Properties)
	{
		std::vector<std::string> Names;
		for (const FProperty* const Property : Properties)
		{
			Names.push_back(Property->GetName());
		}
		return Names;
	}

	/** The saved properties of WTestValueObject, in the order of its declaration: all but TransientValue. */
	const std::vector<std::string> SavedValueProperties = {"bFlag",
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
		"Color",
		"Transform",
		"Numbers",
		"Names",
		"Strong",
		"Weak",
		"StrongArray",
		"PrivateValue"};
}

TEST_SUITE("FObjectProperties")
{
	TEST_CASE("The saved properties of a class are those of its base classes first, in declaration order")
	{
		// A private property is saved like any other, and a Transient one is not.
		CHECK(GetNames(FObjectProperties::GetSaved(*WTestValueObject::StaticClass())) == SavedValueProperties);

		std::vector<std::string> Extended = SavedValueProperties;
		Extended.emplace_back("ExtraValue");
		CHECK(GetNames(FObjectProperties::GetSaved(*WTestExtendedValueObject::StaticClass())) == Extended);

		// A class with no property of its own saves those of its base class.
		CHECK(GetNames(FObjectProperties::GetSaved(*WTestDerivedValueObject::StaticClass())) == SavedValueProperties);
		CHECK(FObjectProperties::GetSaved(*WObject::StaticClass()).empty());
	}

	TEST_CASE("Export writes every saved property, defaults too, and hands references to the context")
	{
		const FScopedObjectTest ObjectTest;
		WTestExtendedValueObject* const Object = NewObject<WTestExtendedValueObject>();
		WTestValueObject* const Other = NewObject<WTestValueObject>();
		Object->Strong = Other;
		Object->Int32Value = 12;
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Other};

		const FJsonValue Exported = FObjectProperties::Export(*Object, Context);

		REQUIRE(Exported.IsObject());
		std::vector<std::string> Expected = SavedValueProperties;
		Expected.emplace_back("ExtraValue");
		CHECK(GetMemberNames(Exported) == Expected);
		CHECK(*Exported.FindMember("Int32Value") == FJsonValue(12));
		// What the class gives a new object is written as well.
		CHECK(*Exported.FindMember("ExtraValue") == FJsonValue(7));
		CHECK(*Exported.FindMember("Text") == FJsonValue(""));
		// The context says how a reference is written: by its index here, and null for one it cannot write.
		CHECK(*Exported.FindMember("Strong") == FJsonValue(0));
		CHECK(Exported.FindMember("Weak")->IsNull());
		CHECK(Exported.FindMember("TransientValue") == nullptr);
	}

	TEST_CASE("Import sets what the JSON names and leaves the rest as it is")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestValueObject* const Other = NewObject<WTestValueObject>();
		Object->FloatValue = 2.5f;
		Object->Text = "Kept";
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Other};
		std::vector<std::string> Problems;

		FObjectProperties::Import(
			*Object, ParseJson(R"({"Int32Value": 5, "Strong": 0, "Numbers": [1, 2]})"), Context, "a test", Problems);

		CHECK(Problems.empty());
		CHECK(Object->Int32Value == 5);
		CHECK(Object->Strong.Get() == Other);
		CHECK(Object->Numbers == std::vector<int32>{1, 2});
		CHECK(Object->FloatValue == 2.5f);
		CHECK(Object->Text == "Kept");

		// An empty object changes nothing.
		const FJsonValue Before = FObjectProperties::Export(*Object, Context);
		FObjectProperties::Import(*Object, FJsonValue::MakeObject(), Context, "a test", Problems);
		CHECK(Problems.empty());
		CHECK(FObjectProperties::Export(*Object, Context) == Before);
	}

	TEST_CASE("What Export writes, Import reads back into another object as it was")
	{
		const FScopedObjectTest ObjectTest;
		WTestExtendedValueObject* const Original = NewObject<WTestExtendedValueObject>();
		WTestValueObject* const Other = NewObject<WTestValueObject>();
		Original->bFlag = true;
		Original->DoubleValue = 1.0e13 + 0.25;
		Original->Text = "Text";
		Original->Location = FVector(1.0, -2.0, 3.0);
		Original->Names = {"a", "b"};
		Original->Strong = Other;
		Original->Weak = Other;
		Original->StrongArray = {Other, nullptr};
		Original->ExtraValue = -3;
		Original->SetPrivateValue(9);
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Other};
		const FJsonValue Exported = FObjectProperties::Export(*Original, Context);

		WTestExtendedValueObject* const Copy = NewObject<WTestExtendedValueObject>();
		std::vector<std::string> Problems;
		FObjectProperties::Import(*Copy, Exported, Context, "a test", Problems);

		CHECK(Problems.empty());
		CHECK(FObjectProperties::Export(*Copy, Context) == Exported);
		CHECK(Copy->GetPrivateValue() == 9);
		CHECK(Copy->Weak.Get() == Other);
	}

	TEST_CASE("A member that cannot be used is a problem, and its property keeps its value while the others are set")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Int32Value = 4;
		Object->TransientValue = 1.0f;
		Object->Numbers = {7};
		FTestPropertyJsonContext Context;
		// The problems are added to what the list already holds.
		std::vector<std::string> Problems = {"Before"};

		FObjectProperties::Import(*Object,
			ParseJson(R"({
				"Colour": [1, 0, 0, 1],
				"FloatValue": 1.5,
				"TransientValue": 2.0,
				"Int32Value": "twelve",
				"Numbers": [1, "two"],
				"Text": "Set"
			})"),
			Context,
			"a test",
			Problems);

		const std::vector<std::string> Expected = {"Before",
			"there is no property named \"Colour\"; the value was ignored",
			"the property \"TransientValue\" is not saved with a test; the value was ignored",
			"property \"Int32Value\": expected a number but found a string",
			"property \"Numbers\": element 1: expected a number but found a string"};
		CHECK(Problems == Expected);
		CHECK(Object->Int32Value == 4);
		CHECK(Object->TransientValue == 1.0f);
		CHECK(Object->Numbers == std::vector<int32>{7});
		CHECK(Object->FloatValue == 1.5f);
		CHECK(Object->Text == "Set");
	}

	TEST_CASE("Saved properties that are not a JSON object are a mistake in the code")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		FTestPropertyJsonContext Context;
		std::vector<std::string> Problems;

		CHECK_THROWS_AS(FObjectProperties::Import(*Object, FJsonValue::MakeArray(), Context, "a test", Problems),
			FFatalErrorException);
		CHECK(Problems.empty());
	}
}
