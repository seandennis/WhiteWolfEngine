#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

namespace
{
	WTestObject* NewRootObject()
	{
		WTestObject* const Root = NewObject<WTestObject>();
		Root->AddToRoot();
		return Root;
	}

	int32 GetAliveCount()
	{
		return FTestObjectStats::Get().AliveCount;
	}
}

TEST_SUITE("WObject::Destroy")
{
	TEST_CASE("IsValid is false for null and true for a new object")
	{
		const FScopedObjectTest ObjectTest;
		const WObject* const Null = nullptr;

		CHECK_FALSE(IsValid(Null));
		CHECK(IsValid(NewObject<WTestObject>()));
		CHECK(IsValid(WTestObject::StaticClass()));
	}

	TEST_CASE("OnDestroyed runs once, during the first Destroy, when the object is already invalid")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();

		CHECK(FTestObjectStats::Get().OnDestroyedCount == 0);

		Object->Destroy();
		CHECK(FTestObjectStats::Get().OnDestroyedCount == 1);
		CHECK(FTestObjectStats::Get().InvalidInOnDestroyedCount == 1);

		Object->Destroy();
		CHECK(FTestObjectStats::Get().OnDestroyedCount == 1);
	}

	TEST_CASE("OnDestroyed is not called for an object that is collected without Destroy")
	{
		const FScopedObjectTest ObjectTest;
		NewObject<WTestObject>();

		CollectGarbage();

		CHECK(FTestObjectStats::Get().DestructorCount == 1);
		CHECK(FTestObjectStats::Get().OnDestroyedCount == 0);
	}

	TEST_CASE("A destroyed object is invalid immediately but stays allocated until the next collection")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Name = "A";

		Object->Destroy();

		CHECK_FALSE(IsValid(Object));
		CHECK(GetAliveCount() == 1);
		CHECK(Object->Name == "A");
		CHECK(FTestObjectStats::Get().BeginDestroyCount == 0);

		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("A destroyed object is collected even though a reachable object refers to it")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		Root->Other = NewObject<WTestObject>();

		Root->Other->Destroy();
		CollectGarbage();

		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("References to a destroyed object are set to null when it is collected")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Other = NewObject<WTestObject>();
		WTestObject* const Doomed = NewObject<WTestObject>();
		Root->Other = Doomed;
		Root->Children.push_back(Other);
		Other->Other = Doomed;

		Doomed->Destroy();

		// Until the collection runs, the references still point at the allocated, invalid object.
		CHECK(Root->Other == Doomed);
		CHECK_FALSE(IsValid(Root->Other));

		CollectGarbage();

		CHECK(Root->Other == nullptr);
		CHECK(Other->Other == nullptr);
		CHECK(GetAliveCount() == 2);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Array elements that refer to a destroyed object become null and keep their place")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Kept = NewObject<WTestObject>();
		WTestObject* const Doomed = NewObject<WTestObject>();
		Root->Children = {Kept, Doomed, Kept, Doomed};

		Doomed->Destroy();
		CollectGarbage();

		REQUIRE(Root->Children.size() == 4);
		CHECK(Root->Children[0] == Kept);
		CHECK(Root->Children[1] == nullptr);
		CHECK(Root->Children[2] == Kept);
		CHECK(Root->Children[3] == nullptr);
		Root->RemoveFromRoot();
	}

	TEST_CASE("A destroyed object does not keep the objects it refers to alive")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Doomed = NewObject<WTestObject>();
		Doomed->Other = NewObject<WTestObject>();
		Doomed->Children.push_back(NewObject<WTestObject>());
		Root->Other = Doomed;

		Doomed->Destroy();
		CollectGarbage();

		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object that a destroyed object refers to survives if something else keeps it alive")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Shared = NewObject<WTestObject>();
		WTestObject* const Doomed = NewObject<WTestObject>();
		Doomed->Other = Shared;
		Root->Other = Doomed;
		Root->Children.push_back(Shared);

		Doomed->Destroy();
		CollectGarbage();

		CHECK(GetAliveCount() == 2);
		CHECK(IsValid(Shared));
		Root->RemoveFromRoot();
	}

	TEST_CASE("Destroying a rooted object removes it from the root set")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();

		Root->Destroy();

		CHECK_FALSE(Root->IsRooted());
		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("Destroying an object twice has no further effect")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();

		Object->Destroy();
		Object->Destroy();
		CollectGarbage();

		CHECK(GetAliveCount() == 0);
		CHECK(FTestObjectStats::Get().BeginDestroyCount == 1);
	}

	TEST_CASE("A destroyed object cannot be added to the root set")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Destroy();

		CHECK_THROWS_AS(Object->AddToRoot(), FFatalErrorException);
		CHECK_FALSE(Object->IsRooted());
	}

	TEST_CASE("Destroying a class is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(WTestObject::StaticClass()->Destroy(), FFatalErrorException);
		CHECK(IsValid(WTestObject::StaticClass()));
	}

	TEST_CASE("During BeginDestroy, objects destroyed in the same collection are not valid")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const First = NewObject<WTestObject>();
		WTestObject* const Second = NewObject<WTestObject>();
		Root->Children = {First, Second};
		First->WeakOther = Second;
		Second->WeakOther = First;

		// Both become garbage in the same collection, one by losing its last reference and one by Destroy.
		Root->Children[0] = nullptr;
		Second->Destroy();
		CollectGarbage();

		CHECK(FTestObjectStats::Get().BeginDestroyCount == 2);
		CHECK(FTestObjectStats::Get().ValidWeakOtherInBeginDestroyCount == 0);
		Root->RemoveFromRoot();
	}
}
