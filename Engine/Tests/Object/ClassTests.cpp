#include "Object/Class.h"

#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/ArrayProperty.h"
#include "Reflection/ObjectProperty.h"
#include "Reflection/PropertyFactory.h"
#include "Reflection/WeakObjectProperty.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <vector>

TEST_SUITE("WClass")
{
	TEST_CASE("WObject and WClass describe each other")
	{
		WClass* const ObjectClass = WObject::StaticClass();
		WClass* const ClassClass = WClass::StaticClass();

		REQUIRE(ObjectClass != nullptr);
		REQUIRE(ClassClass != nullptr);
		CHECK(ObjectClass->GetName() == "WObject");
		CHECK(ObjectClass->GetSuperClass() == nullptr);
		CHECK(ClassClass->GetName() == "WClass");
		CHECK(ClassClass->GetSuperClass() == ObjectClass);

		// A class object is itself an object, and its class is WClass.
		CHECK(ObjectClass->GetClass() == ClassClass);
		CHECK(ClassClass->GetClass() == ClassClass);
		CHECK(ObjectClass->IsA<WObject>());
	}

	TEST_CASE("StaticClass always returns the same class object")
	{
		CHECK(WTestObject::StaticClass() == WTestObject::StaticClass());
		CHECK(WTestObject::StaticClass() != WTestDerivedObject::StaticClass());
	}

	TEST_CASE("A registered class knows its name and base class")
	{
		WClass* const Class = WTestDerivedObject::StaticClass();

		CHECK(Class->GetName() == "WTestDerivedObject");
		CHECK(Class->GetSuperClass() == WTestObject::StaticClass());
		CHECK(Class->GetSuperClass()->GetSuperClass() == WObject::StaticClass());
	}

	TEST_CASE("IsChildOf is true for the class itself and for every base class, and for nothing else")
	{
		WClass* const Derived = WTestDerivedObject::StaticClass();

		CHECK(Derived->IsChildOf(Derived));
		CHECK(Derived->IsChildOf(WTestObject::StaticClass()));
		CHECK(Derived->IsChildOf(WObject::StaticClass()));
		CHECK_FALSE(Derived->IsChildOf(WTestOwnerObject::StaticClass()));
		CHECK_FALSE(WTestObject::StaticClass()->IsChildOf(Derived));
		CHECK_FALSE(Derived->IsChildOf(nullptr));
	}

	TEST_CASE("FindByName finds registered classes by their full name")
	{
		CHECK(WClass::FindByName("WObject") == WObject::StaticClass());
		CHECK(WClass::FindByName("WClass") == WClass::StaticClass());
		CHECK(WClass::FindByName("WTestObject") == WTestObject::StaticClass());

		CHECK(WClass::FindByName("TestObject") == nullptr);
		CHECK(WClass::FindByName("wtestobject") == nullptr);
		CHECK(WClass::FindByName("") == nullptr);
	}

	TEST_CASE("A class is registered when its module loads, before any code names it")
	{
		// Nothing in the tests calls WTestOwnerObject::StaticClass() before this lookup can run.
		CHECK(WClass::FindByName("WTestOwnerObject") != nullptr);
	}

	TEST_CASE("GetAllClasses lists every registered class once, a class after its base class")
	{
		// The engine's own classes, and those that another module, the tests, registered.
		const std::vector<const WClass*> ExpectedClasses = {WObject::StaticClass(),
			WClass::StaticClass(),
			WTestObject::StaticClass(),
			WTestDerivedObject::StaticClass(),
			WTestAbstractObject::StaticClass()};

		const std::vector<WClass*> Classes = WClass::GetAllClasses();

		const auto Find = [&Classes](const WClass* Class)
		{
			return std::ranges::find(Classes, Class);
		};
		for (const WClass* const Expected : ExpectedClasses)
		{
			CAPTURE(Expected->GetName());
			CHECK(std::ranges::count(Classes, Expected) == 1);
		}

		for (const WClass* const Class : Classes)
		{
			REQUIRE(Class != nullptr);
			CAPTURE(Class->GetName());
			CHECK(WClass::FindByName(Class->GetName()) == Class);
			if (Class->GetSuperClass() != nullptr)
			{
				CHECK(Find(Class->GetSuperClass()) < Find(Class));
			}
		}
	}

	TEST_CASE("A class with a pure virtual function is abstract")
	{
		CHECK(WTestAbstractObject::StaticClass()->IsAbstract());
		CHECK_FALSE(WTestObject::StaticClass()->IsAbstract());
		CHECK_FALSE(WObject::StaticClass()->IsAbstract());
	}

	TEST_CASE("WClass itself is abstract, because class objects are created only by registration")
	{
		CHECK(WClass::StaticClass()->IsAbstract());
	}

	TEST_CASE("A class lists the properties it declares, in declaration order")
	{
		const std::vector<std::unique_ptr<FProperty>>& Properties = WTestObject::StaticClass()->GetOwnProperties();

		REQUIRE(Properties.size() == 3);
		CHECK(Properties[0]->GetName() == "Other");
		CHECK(Properties[1]->GetName() == "Children");
		CHECK(Properties[2]->GetName() == "WeakOther");
	}

	TEST_CASE("Own properties do not include inherited ones")
	{
		const std::vector<std::unique_ptr<FProperty>>& Properties =
			WTestDerivedObject::StaticClass()->GetOwnProperties();

		REQUIRE(Properties.size() == 1);
		CHECK(Properties[0]->GetName() == "Extra");
	}

	TEST_CASE("FindProperty finds declared and inherited properties")
	{
		WClass* const Derived = WTestDerivedObject::StaticClass();

		CHECK(Derived->FindProperty("Extra") != nullptr);
		CHECK(Derived->FindProperty("Other") == WTestObject::StaticClass()->FindProperty("Other"));
		CHECK(Derived->FindProperty("Children") != nullptr);

		CHECK(Derived->FindProperty("Unreflected") == nullptr);
		CHECK(Derived->FindProperty("other") == nullptr);
		CHECK(Derived->FindProperty("") == nullptr);
		CHECK(WTestObject::StaticClass()->FindProperty("Extra") == nullptr);
	}

	TEST_CASE("Reference properties include inherited ones, base class first")
	{
		const std::vector<const FProperty*>& References = WTestDerivedObject::StaticClass()->GetReferenceProperties();

		REQUIRE(References.size() == 3);
		CHECK(References[0]->GetName() == "Other");
		CHECK(References[1]->GetName() == "Children");
		CHECK(References[2]->GetName() == "Extra");

		CHECK(WObject::StaticClass()->GetReferenceProperties().empty());
	}

	TEST_CASE("A TObjectPtr member is an object property that knows the class it refers to")
	{
		const FProperty* const Property = WTestObject::StaticClass()->FindProperty("Other");
		REQUIRE(Property != nullptr);

		const FObjectProperty* const ObjectProperty = dynamic_cast<const FObjectProperty*>(Property);
		REQUIRE(ObjectProperty != nullptr);
		CHECK(ObjectProperty->GetPropertyClass() == WTestObject::StaticClass());
		CHECK(ObjectProperty->ContainsObjectReferences());
	}

	TEST_CASE("A TWeakObjectPtr member is a weak object property, which holds no strong reference")
	{
		const FProperty* const Property = WTestObject::StaticClass()->FindProperty("WeakOther");
		REQUIRE(Property != nullptr);

		const FWeakObjectProperty* const WeakProperty = dynamic_cast<const FWeakObjectProperty*>(Property);
		REQUIRE(WeakProperty != nullptr);
		CHECK(WeakProperty->GetPropertyClass() == WTestObject::StaticClass());
		CHECK_FALSE(WeakProperty->ContainsObjectReferences());
	}

	TEST_CASE("A std::vector member is an array property with an element property")
	{
		const FProperty* const Property = WTestObject::StaticClass()->FindProperty("Children");
		REQUIRE(Property != nullptr);

		const FArrayProperty* const ArrayProperty = dynamic_cast<const FArrayProperty*>(Property);
		REQUIRE(ArrayProperty != nullptr);
		CHECK(ArrayProperty->ContainsObjectReferences());
		CHECK(ArrayProperty->GetElementProperty().GetName().empty());

		const FObjectProperty* const ElementProperty =
			dynamic_cast<const FObjectProperty*>(&ArrayProperty->GetElementProperty());
		REQUIRE(ElementProperty != nullptr);
		CHECK(ElementProperty->GetPropertyClass() == WTestObject::StaticClass());
	}

	TEST_CASE("GetValuePtr returns the address of the member inside an object")
	{
		const FScopedObjectTest ObjectTest;
		WTestDerivedObject* const Object = NewObject<WTestDerivedObject>();
		WClass* const Class = Object->GetClass();

		// The inherited members are found through the derived object too.
		CHECK(Class->FindProperty("Other")->GetValuePtr(Object) == &Object->Other);
		CHECK(Class->FindProperty("Children")->GetValuePtr(Object) == &Object->Children);
		CHECK(Class->FindProperty("Extra")->GetValuePtr(Object) == &Object->Extra);
	}

	TEST_CASE("An array property gives access to the elements of an array")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Children.push_back(NewObject<WTestObject>());
		Object->Children.push_back(nullptr);

		const FArrayProperty* const ArrayProperty =
			dynamic_cast<const FArrayProperty*>(Object->GetClass()->FindProperty("Children"));
		REQUIRE(ArrayProperty != nullptr);
		void* const Value = ArrayProperty->GetValuePtr(Object);

		CHECK(ArrayProperty->GetNum(Value) == 2);
		CHECK(ArrayProperty->GetElementPtr(Value, 0) == &Object->Children[0]);
		CHECK(ArrayProperty->GetElementPtr(Value, 1) == &Object->Children[1]);
	}

	TEST_CASE("Accessing an array element out of range is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Children.push_back(nullptr);

		const FArrayProperty* const ArrayProperty =
			dynamic_cast<const FArrayProperty*>(Object->GetClass()->FindProperty("Children"));
		REQUIRE(ArrayProperty != nullptr);
		void* const Value = ArrayProperty->GetValuePtr(Object);

		CHECK_THROWS_AS(ArrayProperty->GetElementPtr(Value, 1), FFatalErrorException);
		CHECK_THROWS_AS(ArrayProperty->GetElementPtr(Value, -1), FFatalErrorException);
	}

	TEST_CASE("The element property of an array has no location of its own")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestObject* const Object = NewObject<WTestObject>();

		const FArrayProperty* const ArrayProperty =
			dynamic_cast<const FArrayProperty*>(Object->GetClass()->FindProperty("Children"));
		REQUIRE(ArrayProperty != nullptr);

		CHECK_THROWS_AS(ArrayProperty->GetElementProperty().GetValuePtr(Object), FFatalErrorException);
	}

	TEST_CASE("Registering a second class with an existing name is a fatal error")
	{
		const FScopedLogCapture Capture;
		FNativeClassDescription Description;
		Description.Name = "WTestObject";
		Description.Size = sizeof(WTestObject);
		Description.Alignment = alignof(WTestObject);
		Description.GetSuperClass = &WObject::StaticClass;

		CHECK_THROWS_AS(WClass::RegisterNativeClass(Description), FFatalErrorException);
		CHECK(WClass::FindByName("WTestObject") == WTestObject::StaticClass());
	}

	TEST_CASE("Registering a class without a name or a base class is a fatal error")
	{
		const FScopedLogCapture Capture;
		FNativeClassDescription Unnamed;
		Unnamed.GetSuperClass = &WObject::StaticClass;
		FNativeClassDescription Rootless;
		Rootless.Name = "WTestRootlessObject";

		CHECK_THROWS_AS(WClass::RegisterNativeClass(Unnamed), FFatalErrorException);
		CHECK_THROWS_AS(WClass::RegisterNativeClass(Rootless), FFatalErrorException);
		CHECK(WClass::FindByName("WTestRootlessObject") == nullptr);
	}

	TEST_CASE("Adding a property to a class after registration is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(WTestObject::StaticClass()->AddProperty(MakeProperty<&WTestObject::Unreflected>("Unreflected")),
			FFatalErrorException);
		CHECK(WTestObject::StaticClass()->FindProperty("Unreflected") == nullptr);
	}

	TEST_CASE("A class whose registration fails is not registered")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		FNativeClassDescription Description;
		Description.Name = "WTestDuplicatePropertyObject";
		Description.Size = sizeof(WTestObject);
		Description.Alignment = alignof(WTestObject);
		Description.GetSuperClass = &WTestObject::StaticClass;
		Description.RegisterMembers = [](WClass& Class)
		{
			// "Other" is already a property of the base class.
			Class.AddProperty(MakeProperty<&WTestObject::Unreflected>("Other"));
		};

		CHECK_THROWS_AS(WClass::RegisterNativeClass(Description), FFatalErrorException);

		CHECK(WClass::FindByName("WTestDuplicatePropertyObject") == nullptr);
	}
}
