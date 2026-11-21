#include "Object/Object.h"

#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

TEST_SUITE("WObject")
{
	TEST_CASE("NewObject creates an object of the requested class")
	{
		const FScopedObjectTest ObjectTest;

		WTestObject* const Object = NewObject<WTestObject>();

		REQUIRE(Object != nullptr);
		CHECK(Object->GetClass() == WTestObject::StaticClass());
		CHECK(Object->Other == nullptr);
		CHECK(Object->Children.empty());
		CHECK(FTestObjectStats::Get().AliveCount == 1);
	}

	TEST_CASE("NewObject can create a plain WObject")
	{
		const FScopedObjectTest ObjectTest;

		WObject* const Object = NewObject<WObject>();

		REQUIRE(Object != nullptr);
		CHECK(Object->GetClass() == WObject::StaticClass());
	}

	TEST_CASE("NewObject creates an instance of a derived class when given one")
	{
		const FScopedObjectTest ObjectTest;

		WTestObject* const Object = NewObject<WTestObject>(WTestDerivedObject::StaticClass());

		REQUIRE(Object != nullptr);
		CHECK(Object->GetClass() == WTestDerivedObject::StaticClass());
		CHECK(Cast<WTestDerivedObject>(Object) != nullptr);
	}

	TEST_CASE("The untyped NewObject creates an instance of any concrete class")
	{
		const FScopedObjectTest ObjectTest;

		WObject* const Object = NewObject(WTestDerivedObject::StaticClass());

		REQUIRE(Object != nullptr);
		CHECK(Object->GetClass() == WTestDerivedObject::StaticClass());
	}

	TEST_CASE("NewObject refuses a class that does not derive from the requested type")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(NewObject<WTestObject>(WTestOwnerObject::StaticClass()), FFatalErrorException);
		CHECK_THROWS_AS(NewObject<WTestDerivedObject>(WTestObject::StaticClass()), FFatalErrorException);
		CHECK(FTestObjectStats::Get().AliveCount == 0);
	}

	TEST_CASE("NewObject refuses a null class")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(NewObject<WTestObject>(nullptr), FFatalErrorException);
		CHECK_THROWS_AS(NewObject(nullptr), FFatalErrorException);
	}

	TEST_CASE("NewObject refuses an abstract class")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(NewObject<WTestAbstractObject>(), FFatalErrorException);
		CHECK_THROWS_AS(NewObject(WTestAbstractObject::StaticClass()), FFatalErrorException);
	}

	TEST_CASE("NewObject refuses to create a class object")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(NewObject<WClass>(), FFatalErrorException);
	}

	TEST_CASE("Constructing an object without NewObject is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(WTestObject(), FFatalErrorException);

		// The failed construction leaves nothing behind.
		CHECK(FTestObjectStats::Get().AliveCount == 0);
	}

	TEST_CASE("A constructor can create other objects")
	{
		const FScopedObjectTest ObjectTest;

		WTestOwnerObject* const Owner = NewObject<WTestOwnerObject>();

		REQUIRE(Owner != nullptr);
		CHECK(Owner->GetClass() == WTestOwnerObject::StaticClass());
		REQUIRE(Owner->Subobject != nullptr);
		CHECK(Owner->Subobject->GetClass() == WTestObject::StaticClass());
	}

	TEST_CASE("IsA is true for the object's class and every base class")
	{
		const FScopedObjectTest ObjectTest;
		WTestDerivedObject* const Object = NewObject<WTestDerivedObject>();

		CHECK(Object->IsA<WTestDerivedObject>());
		CHECK(Object->IsA<WTestObject>());
		CHECK(Object->IsA<WObject>());
		CHECK(Object->IsA(WTestObject::StaticClass()));

		CHECK_FALSE(Object->IsA<WTestOwnerObject>());
		CHECK_FALSE(Object->IsA<WClass>());
		CHECK_FALSE(Object->IsA(nullptr));
	}

	TEST_CASE("IsA is false for a class derived from the object's class")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();

		CHECK_FALSE(Object->IsA<WTestDerivedObject>());
	}

	TEST_CASE("Cast returns the object when it is of the target class, and null otherwise")
	{
		const FScopedObjectTest ObjectTest;
		WObject* const Derived = NewObject<WTestDerivedObject>();
		WObject* const Base = NewObject<WTestObject>();

		CHECK(Cast<WTestDerivedObject>(Derived) == Derived);
		CHECK(Cast<WTestObject>(Derived) == Derived);
		CHECK(Cast<WObject>(Derived) == Derived);

		CHECK(Cast<WTestDerivedObject>(Base) == nullptr);
		CHECK(Cast<WTestOwnerObject>(Base) == nullptr);
	}

	TEST_CASE("Cast of a null pointer is null")
	{
		WObject* const Null = nullptr;
		const WObject* const ConstNull = nullptr;

		CHECK(Cast<WTestObject>(Null) == nullptr);
		CHECK(Cast<WTestObject>(ConstNull) == nullptr);
	}

	TEST_CASE("Cast works on pointers to const objects")
	{
		const FScopedObjectTest ObjectTest;
		const WObject* const Object = NewObject<WTestDerivedObject>();

		const WTestObject* const Result = Cast<WTestObject>(Object);

		CHECK(Result == Object);
	}

	TEST_CASE("A TObjectPtr converts to and from plain pointers")
	{
		const FScopedObjectTest ObjectTest;
		WTestDerivedObject* const Derived = NewObject<WTestDerivedObject>();

		TObjectPtr<WTestDerivedObject> DerivedPtr = Derived;
		TObjectPtr<WTestObject> BasePtr = DerivedPtr;
		WTestObject* const Raw = BasePtr;

		CHECK(DerivedPtr.Get() == Derived);
		CHECK(BasePtr.Get() == Derived);
		CHECK(Raw == Derived);
		CHECK(BasePtr == Derived);
		CHECK(BasePtr != nullptr);
		CHECK(&*BasePtr == Derived);
		CHECK(BasePtr->GetClass() == WTestDerivedObject::StaticClass());

		BasePtr = nullptr;
		CHECK(BasePtr == nullptr);
		CHECK(BasePtr.Get() == nullptr);

		DerivedPtr.Reset();
		CHECK(DerivedPtr == nullptr);
	}

	TEST_CASE("A default-constructed TObjectPtr is null")
	{
		const TObjectPtr<WTestObject> Pointer;
		const TObjectPtr<WTestObject> FromNull = nullptr;

		CHECK(Pointer == nullptr);
		CHECK(FromNull.Get() == nullptr);
	}

	TEST_CASE("An object is not rooted until it is added to the root set")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		CHECK_FALSE(Object->IsRooted());

		Object->AddToRoot();
		CHECK(Object->IsRooted());

		Object->AddToRoot();
		CHECK(Object->IsRooted());

		Object->RemoveFromRoot();
		CHECK_FALSE(Object->IsRooted());

		Object->RemoveFromRoot();
		CHECK_FALSE(Object->IsRooted());
	}

	TEST_CASE("Calling BeginDestroy directly is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestMisbehavingObject* const Object = NewObject<WTestMisbehavingObject>();
		Object->Behavior = WTestMisbehavingObject::EBehavior::CreateObject;

		CHECK_THROWS_AS(Object->CallBeginDestroyDirectly(), FFatalErrorException);
	}
}
