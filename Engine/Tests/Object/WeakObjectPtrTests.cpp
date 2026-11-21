#include "Object/WeakObjectPtr.h"

#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"

#include <doctest/doctest.h>

#include <vector>

TEST_SUITE("TWeakObjectPtr")
{
	TEST_CASE("A default-constructed weak reference is null")
	{
		const TWeakObjectPtr<WTestObject> Weak;
		const TWeakObjectPtr<WTestObject> FromNullLiteral = nullptr;
		WTestObject* const NullObject = nullptr;
		const TWeakObjectPtr<WTestObject> FromNullPointer = NullObject;

		CHECK(Weak.Get() == nullptr);
		CHECK_FALSE(Weak.IsValid());
		CHECK_FALSE(FromNullLiteral.IsValid());
		CHECK_FALSE(FromNullPointer.IsValid());
		CHECK(Weak == FromNullLiteral);
		CHECK(Weak == FromNullPointer);
	}

	TEST_CASE("A weak reference returns its object while the object exists")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();

		const TWeakObjectPtr<WTestObject> Weak = Object;

		CHECK(Weak.IsValid());
		CHECK(Weak.Get() == Object);
	}

	TEST_CASE("A weak reference does not keep its object alive, and is null once the object is destroyed")
	{
		const FScopedObjectTest ObjectTest;
		const TWeakObjectPtr<WTestObject> Weak = NewObject<WTestObject>();
		REQUIRE(Weak.IsValid());

		CollectGarbage();

		CHECK(FTestObjectStats::Get().AliveCount == 0);
		CHECK_FALSE(Weak.IsValid());
		CHECK(Weak.Get() == nullptr);
	}

	TEST_CASE("A weak reference stays valid across collections while something else keeps the object alive")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewObject<WTestObject>();
		Root->AddToRoot();
		Root->Other = NewObject<WTestObject>();
		const TWeakObjectPtr<WTestObject> Weak = Root->Other;

		CollectGarbage();
		CHECK(Weak.Get() == Root->Other);

		Root->Other = nullptr;
		CollectGarbage();
		CHECK_FALSE(Weak.IsValid());
		Root->RemoveFromRoot();
	}

	TEST_CASE("A weak reference never returns a different object that reuses the same slot")
	{
		const FScopedObjectTest ObjectTest;
		const TWeakObjectPtr<WTestObject> Weak = NewObject<WTestObject>();
		CollectGarbage();
		REQUIRE_FALSE(Weak.IsValid());

		// The freed slot is handed to one of these new objects.
		WTestObject* const Root = NewObject<WTestObject>();
		Root->AddToRoot();
		for (int32 Index = 0; Index < 100; ++Index)
		{
			Root->Children.push_back(NewObject<WTestObject>());
		}

		CHECK_FALSE(Weak.IsValid());
		CHECK(Weak.Get() == nullptr);
		Root->RemoveFromRoot();
	}

	TEST_CASE("A reflected weak reference does not keep its object alive either")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewObject<WTestObject>();
		Root->AddToRoot();
		Root->WeakOther = NewObject<WTestObject>();

		CollectGarbage();

		CHECK(FTestObjectStats::Get().AliveCount == 1);
		CHECK_FALSE(Root->WeakOther.IsValid());
		Root->RemoveFromRoot();
	}

	TEST_CASE("Weak references to the same object are equal, and stay equal after the object is gone")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const First = NewObject<WTestObject>();
		WTestObject* const Second = NewObject<WTestObject>();
		const TWeakObjectPtr<WTestObject> WeakFirst = First;
		const TWeakObjectPtr<WTestObject> WeakFirstAgain = First;
		const TWeakObjectPtr<WTestObject> WeakSecond = Second;

		CHECK(WeakFirst == WeakFirstAgain);
		CHECK(WeakFirst != WeakSecond);

		CollectGarbage();

		CHECK(WeakFirst == WeakFirstAgain);
		CHECK(WeakFirst != WeakSecond);
		CHECK(WeakFirst != TWeakObjectPtr<WTestObject>());
	}

	TEST_CASE("Reset makes a weak reference null")
	{
		const FScopedObjectTest ObjectTest;
		TWeakObjectPtr<WTestObject> Weak = NewObject<WTestObject>();

		Weak.Reset();

		CHECK_FALSE(Weak.IsValid());
		CHECK(Weak == TWeakObjectPtr<WTestObject>());
	}

	TEST_CASE("A weak reference converts from strong references and from weak references to derived classes")
	{
		const FScopedObjectTest ObjectTest;
		WTestDerivedObject* const Derived = NewObject<WTestDerivedObject>();
		const TObjectPtr<WTestDerivedObject> Strong = Derived;

		const TWeakObjectPtr<WTestDerivedObject> WeakDerived = Strong;
		const TWeakObjectPtr<WTestObject> WeakBase = WeakDerived;
		const TWeakObjectPtr<WTestObject> WeakBaseFromStrong = Strong;

		CHECK(WeakDerived.Get() == Derived);
		CHECK(WeakBase.Get() == Derived);
		CHECK(WeakBaseFromStrong == WeakBase);
	}

	TEST_CASE("A weak reference to a destroyed object is null immediately")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->AddToRoot();
		const TWeakObjectPtr<WTestObject> Weak = Object;

		Object->Destroy();

		CHECK_FALSE(Weak.IsValid());
		CHECK(FTestObjectStats::Get().AliveCount == 1);
	}

	TEST_CASE("A weak reference made from an already destroyed object is null")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Destroy();

		const TWeakObjectPtr<WTestObject> Weak = Object;

		CHECK_FALSE(Weak.IsValid());
	}

	TEST_CASE("During BeginDestroy, weak references to objects destroyed in the same collection are null")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const First = NewObject<WTestObject>();
		WTestObject* const Second = NewObject<WTestObject>();
		First->WeakOther = Second;
		Second->WeakOther = First;

		CollectGarbage();

		CHECK(FTestObjectStats::Get().BeginDestroyCount == 2);
		CHECK(FTestObjectStats::Get().ValidWeakOtherInBeginDestroyCount == 0);
	}

	TEST_CASE("During BeginDestroy, weak references to surviving objects are still valid")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewObject<WTestObject>();
		Root->AddToRoot();
		NewObject<WTestObject>()->WeakOther = Root;

		CollectGarbage();

		CHECK(FTestObjectStats::Get().BeginDestroyCount == 1);
		CHECK(FTestObjectStats::Get().ValidWeakOtherInBeginDestroyCount == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Weak references can be stored in containers and copied freely")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewObject<WTestObject>();
		Root->AddToRoot();
		std::vector<TWeakObjectPtr<WTestObject>> WeakReferences;
		WeakReferences.push_back(Root);
		WeakReferences.push_back(NewObject<WTestObject>());
		const std::vector<TWeakObjectPtr<WTestObject>> Copies = WeakReferences;

		CollectGarbage();

		CHECK(Copies[0].Get() == Root);
		CHECK_FALSE(Copies[1].IsValid());
		Root->RemoveFromRoot();
	}
}
