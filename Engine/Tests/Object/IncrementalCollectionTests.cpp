#include "Object/GarbageCollector.h"

#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace
{
	/** Long enough that a thousand work units cannot reach its end, whatever order objects are scanned in. */
	constexpr int32 LongChainLength = 10000;

	/** Enough work to be sure the root has been scanned, and far too little to reach the end of a long chain. */
	constexpr int32 WorkUnitsToScanRoot = 1000;

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

	/**
	 * Hangs a chain of Length new objects from Root->Other, each referring to the next through Other, and returns
	 * them in order.
	 */
	std::vector<WTestObject*> AppendChain(WTestObject* Root, int32 Length)
	{
		std::vector<WTestObject*> Chain;
		WTestObject* Tail = Root;
		for (int32 Index = 0; Index < Length; ++Index)
		{
			Tail->Other = NewObject<WTestObject>();
			Tail = Tail->Other;
			Chain.push_back(Tail);
		}
		return Chain;
	}

	void FinishCollection()
	{
		bool bIsFinished = false;
		while (!bIsFinished)
		{
			bIsFinished = FGarbageCollector::AdvanceCollection(std::numeric_limits<int32>::max());
		}
	}

	/** A time budget that never runs out within a test. */
	constexpr double GenerousTimeBudgetSeconds = 3600.0;
}

TEST_SUITE("Incremental garbage collection")
{
	TEST_CASE("A collection run one work unit at a time has the same result as a blocking one")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		Root->Other = NewObject<WTestObject>();
		Root->Children.push_back(NewObject<WTestObject>());
		for (int32 Index = 0; Index < 10; ++Index)
		{
			NewObject<WTestObject>();
		}
		REQUIRE(GetAliveCount() == 13);

		FGarbageCollector::StartCollection();
		int32 StepCount = 0;
		while (!FGarbageCollector::AdvanceCollection(1))
		{
			CHECK(FGarbageCollector::IsCollectionInProgress());
			++StepCount;
		}

		CHECK_FALSE(FGarbageCollector::IsCollectionInProgress());
		CHECK(StepCount > 13);
		CHECK(GetAliveCount() == 3);
		CHECK(FTestObjectStats::Get().BeginDestroyCount == 10);
		Root->RemoveFromRoot();
	}

	TEST_CASE("No collection is in progress until one is started")
	{
		const FScopedObjectTest ObjectTest;

		CHECK_FALSE(FGarbageCollector::IsCollectionInProgress());
		CHECK(FGarbageCollector::AdvanceCollection(100));

		FGarbageCollector::StartCollection();
		CHECK(FGarbageCollector::IsCollectionInProgress());

		// Asking for no work leaves the collection where it is.
		CHECK_FALSE(FGarbageCollector::AdvanceCollection(0));
		CHECK(FGarbageCollector::IsCollectionInProgress());

		FinishCollection();
		CHECK_FALSE(FGarbageCollector::IsCollectionInProgress());
	}

	TEST_CASE("Starting a collection while one is in progress is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		FGarbageCollector::StartCollection();

		CHECK_THROWS_AS(FGarbageCollector::StartCollection(), FFatalErrorException);

		FinishCollection();
	}

	TEST_CASE("A reference moved into an already scanned object keeps its target alive")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		const std::vector<WTestObject*> Chain = AppendChain(Root, LongChainLength);
		WTestObject* const Last = Chain.back();

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		// The collector has finished with Root and has not yet seen Last. Moving the only reference to Last from
		// the unscanned end of the chain into Root must not make the collector lose it.
		Root->Children.push_back(Last);
		Chain[Chain.size() - 2]->Other = nullptr;
		FinishCollection();

		CHECK(GetAliveCount() == LongChainLength + 1);
		REQUIRE(Root->Children.size() == 1);
		CHECK(Root->Children[0] == Last);
		CHECK(IsValid(Last));
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object created during marking survives, with everything it is given a reference to")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		const std::vector<WTestObject*> Chain = AppendChain(Root, LongChainLength);
		WTestObject* const Last = Chain.back();

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		WTestObject* const Fresh = NewObject<WTestObject>();
		Fresh->Other = Last;
		Root->Children.push_back(Fresh);
		Chain[Chain.size() - 2]->Other = nullptr;
		FinishCollection();

		CHECK(GetAliveCount() == LongChainLength + 2);
		CHECK(Fresh->Other == Last);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object reached only through a weak reference survives if it is given a strong one during marking")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		const std::vector<WTestObject*> Chain = AppendChain(Root, LongChainLength);
		const TWeakObjectPtr<WTestObject> WeakLast = Chain.back();

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		Chain[Chain.size() - 2]->Other = nullptr;
		Root->Children.push_back(WeakLast.Get());
		FinishCollection();

		CHECK(GetAliveCount() == LongChainLength + 1);
		CHECK(WeakLast.IsValid());
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object added to the root set during marking survives")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		const std::vector<WTestObject*> Chain = AppendChain(Root, LongChainLength);
		WTestObject* const Last = Chain.back();

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		Last->AddToRoot();
		Chain[Chain.size() - 2]->Other = nullptr;
		FinishCollection();

		CHECK(GetAliveCount() == LongChainLength + 1);
		Last->RemoveFromRoot();
		Root->RemoveFromRoot();
	}

	TEST_CASE("Objects that become unreachable after being marked are collected by the next collection")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		AppendChain(Root, LongChainLength);

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		// The chain was already found reachable, so this collection keeps it.
		Root->Other = nullptr;
		FinishCollection();
		CHECK(GetAliveCount() == LongChainLength + 1);

		CollectGarbage();
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object destroyed during marking before it is reached is collected and its references cleared")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		const std::vector<WTestObject*> Chain = AppendChain(Root, LongChainLength);
		WTestObject* const BeforeLast = Chain[Chain.size() - 2];

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		Chain.back()->Destroy();
		FinishCollection();

		CHECK(GetAliveCount() == LongChainLength);
		CHECK(BeforeLast->Other == nullptr);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object destroyed during marking after it was scanned is collected by the next collection")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		const std::vector<WTestObject*> Chain = AppendChain(Root, LongChainLength);
		WTestObject* const First = Chain.front();

		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		// Root was scanned while First was alive, so Root still refers to it and it cannot be freed yet.
		First->Destroy();
		FinishCollection();
		CHECK(GetAliveCount() == LongChainLength + 1);
		CHECK_FALSE(IsValid(First));
		CHECK(Root->Other == First);

		// The next collection clears the reference, frees the object, and with it the rest of the chain.
		CollectGarbage();
		CHECK(Root->Other == nullptr);
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Storing a reference to a destroyed object stores null")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Doomed = NewObject<WTestObject>();
		Doomed->Destroy();

		Root->Other = Doomed;
		Root->Children.push_back(Doomed);
		const TObjectPtr<WTestObject> Local = Doomed;

		CHECK(Root->Other == nullptr);
		REQUIRE(Root->Children.size() == 1);
		CHECK(Root->Children[0] == nullptr);
		CHECK(Local == nullptr);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Copying a reference to a destroyed object gives null and leaves the original as it was")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const Doomed = NewObject<WTestObject>();
		Root->Other = Doomed;
		Doomed->Destroy();

		const TObjectPtr<WTestObject> Copy = Root->Other;

		CHECK(Copy == nullptr);
		CHECK(Root->Other == Doomed);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Once marking has finished, garbage is invalid and cannot be referenced again")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		WTestObject* const FirstGarbage = NewObject<WTestObject>();
		WTestObject* const SecondGarbage = NewObject<WTestObject>();

		// Step until the first BeginDestroy has run: marking is over and the garbage is being notified.
		FGarbageCollector::StartCollection();
		while (FTestObjectStats::Get().BeginDestroyCount == 0)
		{
			REQUIRE_FALSE(FGarbageCollector::AdvanceCollection(1));
		}
		REQUIRE(FGarbageCollector::IsCollectionInProgress());

		// This test still holds plain pointers to the garbage, which is exactly what engine code must never do.
		CHECK_FALSE(IsValid(FirstGarbage));
		CHECK_FALSE(IsValid(SecondGarbage));
		Root->Other = SecondGarbage;
		CHECK(Root->Other == nullptr);
		CHECK_FALSE(TWeakObjectPtr<WTestObject>(FirstGarbage).IsValid());

		FinishCollection();
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("An object created while garbage is being destroyed survives that collection")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		NewObject<WTestObject>();
		NewObject<WTestObject>();

		FGarbageCollector::StartCollection();
		while (FTestObjectStats::Get().BeginDestroyCount == 0)
		{
			REQUIRE_FALSE(FGarbageCollector::AdvanceCollection(1));
		}

		WTestObject* const Fresh = NewObject<WTestObject>();
		Root->Children.push_back(Fresh);
		FinishCollection();

		CHECK(GetAliveCount() == 2);
		CHECK(IsValid(Fresh));
		Root->RemoveFromRoot();
	}

	TEST_CASE("CollectGarbage finishes a collection in progress and then collects what is unreachable now")
	{
		const FScopedObjectTest ObjectTest;
		WTestObject* const Root = NewRootObject();
		AppendChain(Root, LongChainLength);
		FGarbageCollector::StartCollection();
		FGarbageCollector::AdvanceCollection(WorkUnitsToScanRoot);
		Root->Other = nullptr;

		CollectGarbage();

		CHECK_FALSE(FGarbageCollector::IsCollectionInProgress());
		CHECK(GetAliveCount() == 1);
		Root->RemoveFromRoot();
	}

	TEST_CASE("Tick starts a collection when the interval has passed and finishes it within a generous budget")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedCollectorSettings Settings(
			FGarbageCollectorSettings{.IntervalSeconds = 10.0, .TimeBudgetSeconds = GenerousTimeBudgetSeconds});
		NewObject<WTestObject>();

		FGarbageCollector::Tick(4.0);
		FGarbageCollector::Tick(4.0);
		CHECK(GetAliveCount() == 1);

		FGarbageCollector::Tick(4.0);
		CHECK(GetAliveCount() == 0);
		CHECK_FALSE(FGarbageCollector::IsCollectionInProgress());
	}

	TEST_CASE("The interval is measured from the end of the previous collection")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedCollectorSettings Settings(
			FGarbageCollectorSettings{.IntervalSeconds = 10.0, .TimeBudgetSeconds = GenerousTimeBudgetSeconds});

		FGarbageCollector::Tick(10.0);
		NewObject<WTestObject>();

		FGarbageCollector::Tick(9.0);
		CHECK(GetAliveCount() == 1);

		FGarbageCollector::Tick(1.0);
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("RequestCollection makes the next Tick collect without waiting for the interval")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedCollectorSettings Settings(
			FGarbageCollectorSettings{.IntervalSeconds = 1.0e9, .TimeBudgetSeconds = GenerousTimeBudgetSeconds});
		NewObject<WTestObject>();

		FGarbageCollector::Tick(1.0);
		CHECK(GetAliveCount() == 1);

		FGarbageCollector::RequestCollection();
		FGarbageCollector::Tick(0.0);
		CHECK(GetAliveCount() == 0);

		// The request was for one collection only.
		NewObject<WTestObject>();
		FGarbageCollector::Tick(1.0);
		CHECK(GetAliveCount() == 1);
	}

	TEST_CASE("With an interval of zero every Tick collects")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedCollectorSettings Settings(
			FGarbageCollectorSettings{.IntervalSeconds = 0.0, .TimeBudgetSeconds = GenerousTimeBudgetSeconds});

		for (int32 Round = 0; Round < 3; ++Round)
		{
			NewObject<WTestObject>();
			FGarbageCollector::Tick(0.0);
			CHECK(GetAliveCount() == 0);
		}
	}

	TEST_CASE("A collection that does not fit in the time budget is spread over several Ticks")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedCollectorSettings Settings(
			FGarbageCollectorSettings{.IntervalSeconds = 0.0, .TimeBudgetSeconds = 1.0e-9});
		constexpr int32 GarbageCount = 20000;
		for (int32 Index = 0; Index < GarbageCount; ++Index)
		{
			NewObject<WTestObject>();
		}

		// A budget of a nanosecond is spent by the first batch of work, which is far too small for all of this.
		FGarbageCollector::Tick(0.0);
		CHECK(FGarbageCollector::IsCollectionInProgress());
		CHECK(GetAliveCount() > 0);

		int32 TickCount = 1;
		while (FGarbageCollector::IsCollectionInProgress())
		{
			FGarbageCollector::Tick(0.0);
			++TickCount;
			REQUIRE(TickCount < 100000);
		}

		CHECK(TickCount > 2);
		CHECK(GetAliveCount() == 0);
	}

	TEST_CASE("Invalid collector settings and time steps are fatal errors")
	{
		const FScopedLogCapture Capture;
		const FGarbageCollectorSettings Original = FGarbageCollector::GetSettings();

		CHECK_THROWS_AS(FGarbageCollector::SetSettings(
							FGarbageCollectorSettings{.IntervalSeconds = -1.0, .TimeBudgetSeconds = 1.0}),
			FFatalErrorException);
		CHECK_THROWS_AS(
			FGarbageCollector::SetSettings(FGarbageCollectorSettings{.IntervalSeconds = 1.0, .TimeBudgetSeconds = 0.0}),
			FFatalErrorException);
		CHECK_THROWS_AS(FGarbageCollector::Tick(-0.5), FFatalErrorException);

		CHECK(FGarbageCollector::GetSettings().IntervalSeconds == Original.IntervalSeconds);
		CHECK(FGarbageCollector::GetSettings().TimeBudgetSeconds == Original.TimeBudgetSeconds);
	}

	TEST_CASE("The default settings collect once a minute within two milliseconds per frame")
	{
		const FGarbageCollectorSettings Defaults;

		CHECK(Defaults.IntervalSeconds == 60.0);
		CHECK(Defaults.TimeBudgetSeconds == 0.002);
	}

	TEST_CASE("GetObjectCount follows objects as they are created and destroyed")
	{
		const FScopedObjectTest ObjectTest;
		const int32 Baseline = FGarbageCollector::GetObjectCount();
		CHECK(Baseline > 0);

		for (int32 Index = 0; Index < 3; ++Index)
		{
			NewObject<WTestObject>();
		}
		CHECK(FGarbageCollector::GetObjectCount() == Baseline + 3);

		CollectGarbage();
		CHECK(FGarbageCollector::GetObjectCount() == Baseline);
	}

	TEST_CASE("ReportLeakedObjects reports every object that is still alive and is not a class")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestObject* const Root = NewRootObject();
		Root->Other = NewObject<WTestObject>();
		NewObject<WTestObject>();

		CHECK(FGarbageCollector::ReportLeakedObjects() == 2);

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 2);
		int32 RootedCount = 0;
		for (const FLogMessage& Message : Messages)
		{
			CHECK(Message.Verbosity == ELogVerbosity::Error);
			CHECK(Message.Category == "LogObject");
			CHECK(Message.Text.find("WTestObject") != std::string::npos);
			if (Message.Text.find("never removed from the root set") != std::string::npos)
			{
				++RootedCount;
			}
		}
		CHECK(RootedCount == 1);

		Root->RemoveFromRoot();
		CHECK(FGarbageCollector::ReportLeakedObjects() == 0);
	}
}
