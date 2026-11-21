#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{
	/** Creates a rooted object, so that tests have something reachable to hang other objects from. */
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

TEST_SUITE("Garbage collection")
{
	TEST_CASE("An object that nothing refers to is destroyed")
	{
		const FScopedObjectTest ObjectTest;
		NewObject<WTestObject>();
		REQUIRE(GetAliveCount() == 1);

		CollectGarbage();

		CHECK(GetAliveCount() == 0);
		CHECK(FTestObjectStats::Get().BeginDestroyCount == 1);
		CHECK(FTestObjectStats::Get().DestructorCount == 1);
	}

	TEST_CASE("Collecting when there is no garbage destroys nothing")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();

		CollectGarbage();
		CollectGarbage();

		CHECK(GetAliveCount() == 1);
		CHECK(FTestObjectStats::Get().BeginDestroyCount == 0);
		Root->RemoveFromRoot();
	}

	TEST_CASE("A rooted object survives until it is removed from the root set")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();

		CollectGarbage();
		CHECK(GetAliveCount() == 1);

		Root->RemoveFromRoot();
		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("An object referenced by a reflected property of a reachable object survives")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		Root->Other = NewObject<WTestObject>();

		CollectGarbage();

		CHECK(GetAliveCount() == 2);
		CHECK(Root->Other != nullptr);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object is destroyed once the last reference to it is cleared")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		Root->Other = NewObject<WTestObject>();
		CollectGarbage();
		REQUIRE(GetAliveCount() == 2);

		Root->Other = nullptr;
		CollectGarbage();

		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Reachability follows chains of references")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		Root->Other = NewObject<WTestObject>();
		Root->Other->Other = NewObject<WTestObject>();
		Root->Other->Other->Other = NewObject<WTestObject>();

		CollectGarbage();
		CHECK(GetAliveCount() == 4);

		// Cutting the chain in the middle releases everything after the cut.
		Root->Other->Other = nullptr;
		CollectGarbage();
		CHECK(GetAliveCount() == 2);
		Root->RemoveFromRoot();
	}

	TEST_CASE("A reference that is not a reflected property does not keep its target alive")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		Root->Unreflected = NewObject<WTestObject>();

		CollectGarbage();

		CHECK(GetAliveCount() == 1);
		Root->Unreflected = nullptr;
		Root->RemoveFromRoot();
	}

	TEST_CASE("Objects in a reflected array survive, and are destroyed when removed from it")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Root->Children.push_back(NewObject<WTestObject>());
		}
		Root->Children.push_back(nullptr);

		CollectGarbage();
		CHECK(GetAliveCount() == 6);

		Root->Children.erase(Root->Children.begin(), Root->Children.begin() + 2);
		CollectGarbage();
		CHECK(GetAliveCount() == 4);

		Root->Children.clear();
		CollectGarbage();
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("References held by objects inside an array are followed")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Child = NewObject<WTestObject>();
		Child->Other = NewObject<WTestObject>();
		Child->Children.push_back(NewObject<WTestObject>());
		Root->Children.push_back(Child);

		CollectGarbage();

		CHECK(GetAliveCount() == 4);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Reference properties inherited from a base class are followed")
	{
		const FScopedObjectTest ObjectTest;
		WTestDerivedObject* const Root = NewObject<WTestDerivedObject>();
		Root->AddToRoot();
		Root->Other = NewObject<WTestObject>();
		Root->Children.push_back(NewObject<WTestObject>());
		Root->Extra = NewObject<WTestObject>();

		CollectGarbage();

		CHECK(GetAliveCount() == 4);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Two unreachable objects that refer to each other are both destroyed")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const First = NewObject<WTestObject>();
		WTestObject* const Second = NewObject<WTestObject>();
		First->Other = Second;
		Second->Other = First;

		CollectGarbage();

		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("A cycle that is reachable survives, and is destroyed when it is cut off")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const First = NewObject<WTestObject>();
		WTestObject* const Second = NewObject<WTestObject>();
		First->Other = Second;
		Second->Other = First;
		Second->Children.push_back(First);
		Root->Other = First;

		CollectGarbage();
		CHECK(GetAliveCount() == 3);

		Root->Other = nullptr;
		CollectGarbage();
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An unreachable object that refers to itself is destroyed")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Other = Object;
		Object->Children.push_back(Object);

		CollectGarbage();

		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("An unreachable object that refers to a reachable one does not disturb it")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Garbage = NewObject<WTestObject>();
		Garbage->Other = Root;

		CollectGarbage();

		CHECK(GetAliveCount() == 1);
		CHECK(Root->GetClass() == WTestObject::StaticClass());
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object shared by several owners survives until the last one lets go")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Shared = NewObject<WTestObject>();
		Root->Other = Shared;
		Root->Children.push_back(Shared);
		Root->Children.push_back(Shared);

		Root->Other = nullptr;
		CollectGarbage();
		CHECK(GetAliveCount() == 2);

		Root->Children.clear();
		CollectGarbage();
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object created by a constructor survives with its owner")
	{
		const FScopedObjectTest ObjectTest;
		WTestOwnerObject* const Owner = NewObject<WTestOwnerObject>();
		Owner->AddToRoot();

		CollectGarbage();
		CHECK(GetAliveCount() == 1);

		Owner->RemoveFromRoot();
		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("A very long chain of objects is collected without exhausting the stack")
	{
		const FScopedObjectTest ObjectTest;
		constexpr int32 ChainLength = 200000;
		WTestObject* const Root = NewRootObject();
		WTestObject* Tail = Root;
		for (int32 Index = 1; Index < ChainLength; ++Index)
		{
			Tail->Other = NewObject<WTestObject>();
			Tail = Tail->Other;
		}

		CollectGarbage();
		CHECK(GetAliveCount() == ChainLength);

		Root->RemoveFromRoot();
		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("Many rounds of creating and collecting objects leave nothing behind")
	{
		const FScopedObjectTest ObjectTest;
		constexpr int32 RoundCount = 50;
		constexpr int32 ObjectsPerRound = 2000;

		for (int32 Round = 0; Round < RoundCount; ++Round)
		{
			for (int32 Index = 0; Index < ObjectsPerRound; ++Index)
			{
				NewObject<WTestObject>();
			}
			CollectGarbage();
			REQUIRE(GetAliveCount() == 0);
		}

		CHECK(FTestObjectStats::Get().DestructorCount == RoundCount * ObjectsPerRound);
	}

	TEST_CASE("BeginDestroy is called exactly once per destroyed object, before its destructor")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Object = NewObject<WTestObject>();
		Object->Name = "A";

		CollectGarbage();
		CollectGarbage();

		const std::vector<std::string> Expected = {"BeginDestroy A", "Destructor A"};
		CHECK(FTestObjectStats::Get().Events == Expected);
	}

	TEST_CASE("Every object being destroyed gets BeginDestroy before any of them is destructed")
	{
		const FScopedObjectTest ObjectTest;
		for (const char* const Name : {"A", "B", "C"})
		{
			NewObject<WTestObject>()->Name = Name;
		}

		CollectGarbage();

		const std::vector<std::string>& Events = FTestObjectStats::Get().Events;
		REQUIRE(Events.size() == 6);
		for (std::size_t Index = 0; Index < 3; ++Index)
		{
			CHECK(Events[Index].starts_with("BeginDestroy "));
			CHECK(Events[Index + 3].starts_with("Destructor "));
		}
	}

	TEST_CASE("BeginDestroy can still use the other objects being destroyed with it")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const First = NewObject<WTestObject>();
		WTestObject* const Second = NewObject<WTestObject>();
		First->Other = Second;
		Second->Other = First;

		// Both objects are garbage. Neither may be freed until both have had BeginDestroy, which the address
		// sanitizer verifies when the second BeginDestroy runs.
		CollectGarbage();

		CHECK(FTestObjectStats::Get().BeginDestroyCount == 2);
	}

	TEST_CASE("An object created during BeginDestroy survives that collection")
	{
		const FScopedObjectTest ObjectTest;
		NewObject<WTestMisbehavingObject>()->Behavior = WTestMisbehavingObject::EBehavior::CreateObject;

		CollectGarbage();
		CHECK(GetAliveCount() == 1);

		// Nothing refers to it, so the next collection destroys it.
		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("A BeginDestroy override that does not call its Super is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		NewObject<WTestMisbehavingObject>()->Behavior = WTestMisbehavingObject::EBehavior::SkipSuper;

		CHECK_THROWS_AS(CollectGarbage(), FFatalErrorException);

		// The object system is still usable afterwards, and the offending object is destroyed.
		NewObject<WTestObject>();
		CollectGarbage();
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("Calling CollectGarbage from BeginDestroy is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		NewObject<WTestMisbehavingObject>()->Behavior = WTestMisbehavingObject::EBehavior::CollectGarbage;

		CHECK_THROWS_AS(CollectGarbage(), FFatalErrorException);
	}

	TEST_CASE("An object cannot save itself by joining the root set from BeginDestroy")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		NewObject<WTestMisbehavingObject>()->Behavior = WTestMisbehavingObject::EBehavior::AddSelfToRoot;

		CHECK_THROWS_AS(CollectGarbage(), FFatalErrorException);
	}

	TEST_CASE("Classes are never collected")
	{
		const FScopedObjectTest ObjectTest;

		CollectGarbage();

		CHECK(WClass::FindByName("WTestObject") == WTestObject::StaticClass());
		CHECK(WTestObject::StaticClass()->GetName() == "WTestObject");
		CHECK(WTestObject::StaticClass()->IsRooted());
	}
}
