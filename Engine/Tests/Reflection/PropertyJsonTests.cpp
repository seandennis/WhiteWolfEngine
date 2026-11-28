#include "Reflection/Property.h"

#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/TestReflectionObjects.h"

#include <doctest/doctest.h>

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

	FJsonValue Export(const WTestValueObject& Object, std::string_view Name, FTestPropertyJsonContext& Context)
	{
		const FProperty& Property = FindValueProperty(Name);
		return Property.ExportJson(Property.GetValuePtr(&Object), Context);
	}

	bool Import(WTestValueObject& Object,
		std::string_view Name,
		const FJsonValue& Json,
		FTestPropertyJsonContext& Context,
		std::string& OutError)
	{
		const FProperty& Property = FindValueProperty(Name);
		return Property.ImportJson(Property.GetValuePtr(&Object), Json, Context, OutError);
	}
}

TEST_SUITE("FProperty JSON")
{
	TEST_CASE("Reference and array properties report their C++ types")
	{
		CHECK(FindValueProperty("Strong").GetTypeName() == "TObjectPtr<WTestValueObject>");
		CHECK(FindValueProperty("Weak").GetTypeName() == "TWeakObjectPtr<WTestValueObject>");
		CHECK(FindValueProperty("Numbers").GetTypeName() == "std::vector<int32>");
		CHECK(FindValueProperty("Names").GetTypeName() == "std::vector<std::string>");
		CHECK(FindValueProperty("StrongArray").GetTypeName() == "std::vector<TObjectPtr<WTestValueObject>>");
	}

	TEST_CASE("A null reference exports as null")
	{
		const FScopedObjectTest ObjectTest;
		FTestPropertyJsonContext Context;
		WTestValueObject* const Object = NewObject<WTestValueObject>();

		CHECK(Export(*Object, "Strong", Context).IsNull());
		CHECK(Export(*Object, "Weak", Context).IsNull());
	}

	TEST_CASE("A reference exports as whatever the context writes for the object")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestValueObject* const Other = NewObject<WTestValueObject>();
		Object->Strong = Other;
		Object->Weak = Other;
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object, Other};

		CHECK(Export(*Object, "Strong", Context) == FJsonValue(1));
		CHECK(Export(*Object, "Weak", Context) == FJsonValue(1));
	}

	TEST_CASE("A reference to an object the context does not know exports as null")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Strong = NewObject<WTestValueObject>();
		FTestPropertyJsonContext Context;

		CHECK(Export(*Object, "Strong", Context).IsNull());
	}

	TEST_CASE("A reference to a destroyed object exports as null")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestValueObject* const Other = NewObject<WTestValueObject>();
		Object->Strong = Other;
		Object->Weak = Other;
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object, Other};

		Other->Destroy();

		CHECK(Export(*Object, "Strong", Context).IsNull());
		CHECK(Export(*Object, "Weak", Context).IsNull());
	}

	TEST_CASE("Importing a reference finds the object through the context")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestValueObject* const Other = NewObject<WTestDerivedValueObject>();
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object, Other};
		std::string Error;

		CHECK(Import(*Object, "Strong", FJsonValue(1), Context, Error));
		CHECK(Import(*Object, "Weak", FJsonValue(0), Context, Error));

		CHECK(Object->Strong.Get() == Other);
		CHECK(Object->Weak.Get() == Object);
	}

	TEST_CASE("Importing null clears a reference")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Strong = Object;
		Object->Weak = Object;
		FTestPropertyJsonContext Context;
		std::string Error;

		CHECK(Import(*Object, "Strong", FJsonValue(), Context, Error));
		CHECK(Import(*Object, "Weak", FJsonValue(), Context, Error));

		CHECK(Object->Strong.Get() == nullptr);
		CHECK(Object->Weak.Get() == nullptr);
	}

	TEST_CASE("Importing a reference the context cannot resolve fails and keeps the old reference")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Strong = Object;
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object};
		std::string Error;

		CHECK_FALSE(Import(*Object, "Strong", FJsonValue(5), Context, Error));

		CHECK(Error == "there is no object 5");
		CHECK(Object->Strong.Get() == Object);
	}

	TEST_CASE("Importing a reference to an object of the wrong class fails")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestObject* const WrongClass = NewObject<WTestObject>();
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {WrongClass};
		std::string Error;

		CHECK_FALSE(Import(*Object, "Strong", FJsonValue(0), Context, Error));
		CHECK(Error == "the reference is to a WTestObject, which is not a WTestValueObject");
		CHECK_FALSE(Import(*Object, "Weak", FJsonValue(0), Context, Error));

		CHECK(Object->Strong.Get() == nullptr);
		CHECK(Object->Weak.Get() == nullptr);
	}

	TEST_CASE("An array exports its elements in order")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Numbers = {3, 1, 2};
		Object->Names = {"b", "a"};
		Object->StrongArray = {Object, nullptr};
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object};

		CHECK(Export(*Object, "Numbers", Context) ==
			FJsonValue(FJsonValue::FArray{FJsonValue(3), FJsonValue(1), FJsonValue(2)}));
		CHECK(Export(*Object, "Names", Context) == FJsonValue(FJsonValue::FArray{FJsonValue("b"), FJsonValue("a")}));
		CHECK(Export(*Object, "StrongArray", Context) == FJsonValue(FJsonValue::FArray{FJsonValue(0), FJsonValue()}));
	}

	TEST_CASE("An empty array exports as an empty JSON array")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		FTestPropertyJsonContext Context;

		CHECK(Export(*Object, "Numbers", Context) == FJsonValue::MakeArray());
	}

	TEST_CASE("Importing an array replaces its elements")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Numbers = {9, 9, 9, 9, 9};
		Object->StrongArray = {nullptr};
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object};
		std::string Error;

		CHECK(Import(*Object, "Numbers", FJsonValue(FJsonValue::FArray{FJsonValue(1), FJsonValue(2)}), Context, Error));
		CHECK(Import(*Object,
			"StrongArray",
			FJsonValue(FJsonValue::FArray{FJsonValue(0), FJsonValue(), FJsonValue(0)}),
			Context,
			Error));

		CHECK(Object->Numbers == std::vector<int32>{1, 2});
		REQUIRE(Object->StrongArray.size() == 3);
		CHECK(Object->StrongArray[0].Get() == Object);
		CHECK(Object->StrongArray[1].Get() == nullptr);
		CHECK(Object->StrongArray[2].Get() == Object);
	}

	TEST_CASE("Importing an empty JSON array empties the array")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Names = {"x"};
		FTestPropertyJsonContext Context;
		std::string Error;

		CHECK(Import(*Object, "Names", FJsonValue::MakeArray(), Context, Error));

		CHECK(Object->Names.empty());
	}

	TEST_CASE("An array rejects JSON that is not an array and keeps its elements")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Numbers = {4};
		FTestPropertyJsonContext Context;
		std::string Error;

		CHECK_FALSE(Import(*Object, "Numbers", FJsonValue(4), Context, Error));

		CHECK(Error == "expected an array but found a number");
		CHECK(Object->Numbers == std::vector<int32>{4});
	}

	TEST_CASE("An array reports which element could not be imported, and keeps the elements it had")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Numbers = {7, 8};
		Object->StrongArray = {Object};
		FTestPropertyJsonContext Context;
		Context.KnownObjects = {Object};
		std::string Error;

		CHECK_FALSE(Import(*Object,
			"Numbers",
			FJsonValue(FJsonValue::FArray{FJsonValue(1), FJsonValue("two"), FJsonValue(3)}),
			Context,
			Error));

		CHECK(Error == "element 1: expected a number but found a string");
		CHECK(Object->Numbers == std::vector<int32>{7, 8});

		// A reference that cannot be found, after one that can: the array is read whole or not at all.
		CHECK_FALSE(Import(*Object,
			"StrongArray",
			FJsonValue(FJsonValue::FArray{FJsonValue(), FJsonValue(0), FJsonValue(5)}),
			Context,
			Error));
		CHECK(Error == "element 2: there is no object 5");
		REQUIRE(Object->StrongArray.size() == 1);
		CHECK(Object->StrongArray[0].Get() == Object);
	}
}
