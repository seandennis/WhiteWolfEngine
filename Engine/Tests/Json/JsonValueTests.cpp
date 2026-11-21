#include "Json/JsonValue.h"

#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <limits>
#include <ostream>
#include <string>
#include <string_view>

TEST_SUITE("FJsonValue")
{
	TEST_CASE("A default-constructed value is null")
	{
		const FJsonValue Value;

		CHECK(Value.IsNull());
		CHECK(Value.GetTypeDescription() == "null");
		CHECK(Value == FJsonValue(nullptr));
	}

	TEST_CASE("Each kind of value reports its own type and no other")
	{
		const FJsonValue Boolean(true);
		const FJsonValue Integer(7);
		const FJsonValue Double(2.5);
		const FJsonValue String("text");
		const FJsonValue Array = FJsonValue::MakeArray();
		const FJsonValue Object = FJsonValue::MakeObject();

		CHECK(Boolean.IsBoolean());
		CHECK_FALSE(Boolean.IsNumber());
		CHECK(Boolean.GetTypeDescription() == "a boolean");

		CHECK(Integer.IsInteger());
		CHECK(Integer.IsNumber());
		CHECK_FALSE(Integer.IsBoolean());
		CHECK(Integer.GetTypeDescription() == "a number");

		CHECK(Double.IsNumber());
		CHECK_FALSE(Double.IsInteger());
		CHECK(Double.GetTypeDescription() == "a number");

		CHECK(String.IsString());
		CHECK(String.GetTypeDescription() == "a string");

		CHECK(Array.IsArray());
		CHECK_FALSE(Array.IsObject());
		CHECK(Array.GetTypeDescription() == "an array");

		CHECK(Object.IsObject());
		CHECK_FALSE(Object.IsArray());
		CHECK(Object.GetTypeDescription() == "an object");
	}

	TEST_CASE("Values return what they were constructed from")
	{
		CHECK(FJsonValue(true).GetBoolean());
		CHECK_FALSE(FJsonValue(false).GetBoolean());
		CHECK(FJsonValue(-12).GetInteger() == -12);
		CHECK(FJsonValue(std::numeric_limits<int64>::min()).GetInteger() == std::numeric_limits<int64>::min());
		CHECK(FJsonValue(uint8{200}).GetInteger() == 200);
		CHECK(FJsonValue(2.5).GetNumber() == 2.5);
		CHECK(FJsonValue(7).GetNumber() == 7.0);
		CHECK(FJsonValue(std::string("owned")).GetString() == "owned");
		CHECK(FJsonValue(std::string_view("view")).GetString() == "view");
		CHECK(FJsonValue("literal").GetString() == "literal");
	}

	TEST_CASE("A string literal makes a string, not a boolean")
	{
		const FJsonValue Value("true");

		CHECK(Value.IsString());
	}

	TEST_CASE("An unsigned integer beyond the signed 64-bit range is stored exactly")
	{
		const FJsonValue Value(std::numeric_limits<uint64>::max());

		CHECK(Value.IsUnsignedInteger());
		CHECK(Value.IsNumber());
		CHECK_FALSE(Value.IsInteger());
		CHECK(Value.GetUnsignedInteger() == std::numeric_limits<uint64>::max());
		CHECK(Value.GetNumber() == static_cast<double>(std::numeric_limits<uint64>::max()));
	}

	TEST_CASE("An unsigned integer that fits the signed range equals the same signed integer")
	{
		const FJsonValue Unsigned(uint64{42});

		CHECK(Unsigned.IsInteger());
		CHECK_FALSE(Unsigned.IsUnsignedInteger());
		CHECK(Unsigned == FJsonValue(42));
	}

	TEST_CASE("A float is stored as the double with the same shortest decimal form")
	{
		const FJsonValue Value(0.1f);

		CHECK(Value.GetNumber() == 0.1);
		CHECK(static_cast<float>(Value.GetNumber()) == 0.1f);
	}

	TEST_CASE("A float converts back to exactly the same float")
	{
		for (const float Original : {0.0f, -0.0f, 1.0f, 3.14159274f, 1.0e-30f, 3.4028235e38f, 16777217.0f, -123.456f})
		{
			const FJsonValue Value(Original);

			CHECK(static_cast<float>(Value.GetNumber()) == Original);
		}
	}

	TEST_CASE("A non-finite float stays non-finite")
	{
		const FJsonValue Value(std::numeric_limits<float>::infinity());

		CHECK(Value.GetNumber() == std::numeric_limits<double>::infinity());
	}

	TEST_CASE("An array keeps its elements in order")
	{
		FJsonValue Array = FJsonValue::MakeArray();
		Array.AddElement(1);
		Array.AddElement("two");
		Array.AddElement(FJsonValue());

		REQUIRE(Array.GetElements().size() == 3);
		CHECK(Array.GetElements()[0].GetInteger() == 1);
		CHECK(Array.GetElements()[1].GetString() == "two");
		CHECK(Array.GetElements()[2].IsNull());
	}

	TEST_CASE("An object keeps its members in the order they were added")
	{
		FJsonValue Object = FJsonValue::MakeObject();
		Object.SetMember("Zebra", 1);
		Object.SetMember("Apple", 2);
		Object.SetMember("Mango", 3);

		REQUIRE(Object.GetMembers().size() == 3);
		CHECK(Object.GetMembers()[0].first == "Zebra");
		CHECK(Object.GetMembers()[1].first == "Apple");
		CHECK(Object.GetMembers()[2].first == "Mango");
	}

	TEST_CASE("Setting an existing member replaces its value and keeps its position")
	{
		FJsonValue Object = FJsonValue::MakeObject();
		Object.SetMember("First", 1);
		Object.SetMember("Second", 2);

		Object.SetMember("First", "replaced");

		REQUIRE(Object.GetMembers().size() == 2);
		CHECK(Object.GetMembers()[0].first == "First");
		CHECK(Object.GetMembers()[0].second.GetString() == "replaced");
	}

	TEST_CASE("FindMember returns the member with the key, or null")
	{
		FJsonValue Object = FJsonValue::MakeObject();
		Object.SetMember("Name", "Door");

		const FJsonValue* const Found = Object.FindMember("Name");
		REQUIRE(Found != nullptr);
		CHECK(Found->GetString() == "Door");
		CHECK(Object.FindMember("name") == nullptr);
		CHECK(Object.FindMember("") == nullptr);
	}

	TEST_CASE("Values compare by type and content")
	{
		FJsonValue First = FJsonValue::MakeObject();
		First.SetMember("A", 1);
		First.SetMember("B", FJsonValue(FJsonValue::FArray{FJsonValue(1), FJsonValue("x")}));

		FJsonValue Same = FJsonValue::MakeObject();
		Same.SetMember("A", 1);
		Same.SetMember("B", FJsonValue(FJsonValue::FArray{FJsonValue(1), FJsonValue("x")}));

		FJsonValue Reordered = FJsonValue::MakeObject();
		Reordered.SetMember("B", FJsonValue(FJsonValue::FArray{FJsonValue(1), FJsonValue("x")}));
		Reordered.SetMember("A", 1);

		CHECK(First == Same);
		CHECK_FALSE(First == Reordered);
		CHECK_FALSE(FJsonValue(1) == FJsonValue(1.0));
		CHECK_FALSE(FJsonValue(false) == FJsonValue());
	}

	TEST_CASE("Reading a value as another type is a fatal error")
	{
		const FScopedLogCapture LogCapture;
		FJsonValue String("text");
		const FJsonValue Double(1.5);

		CHECK_THROWS_AS(String.GetBoolean(), FFatalErrorException);
		CHECK_THROWS_AS(String.GetInteger(), FFatalErrorException);
		CHECK_THROWS_AS(String.GetNumber(), FFatalErrorException);
		CHECK_THROWS_AS(String.GetElements(), FFatalErrorException);
		CHECK_THROWS_AS(String.GetMembers(), FFatalErrorException);
		CHECK_THROWS_AS(String.FindMember("Key"), FFatalErrorException);
		CHECK_THROWS_AS(String.AddElement(1), FFatalErrorException);
		CHECK_THROWS_AS(String.SetMember("Key", 1), FFatalErrorException);
		CHECK_THROWS_AS(Double.GetInteger(), FFatalErrorException);
		CHECK_THROWS_AS(Double.GetUnsignedInteger(), FFatalErrorException);
		CHECK_THROWS_AS(FJsonValue(1).GetUnsignedInteger(), FFatalErrorException);
		CHECK_THROWS_AS(FJsonValue(std::numeric_limits<uint64>::max()).GetInteger(), FFatalErrorException);
		CHECK_THROWS_AS(Double.GetString(), FFatalErrorException);
	}
}
