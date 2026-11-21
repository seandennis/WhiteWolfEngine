#include "Object/TestObjects.h"

#include "Object/ObjectGlobals.h"

FTestObjectStats& FTestObjectStats::Get()
{
	static FTestObjectStats Stats;
	return Stats;
}

WTestObject::WTestObject()
{
	++FTestObjectStats::Get().AliveCount;
}

WTestObject::~WTestObject()
{
	FTestObjectStats& Stats = FTestObjectStats::Get();
	--Stats.AliveCount;
	++Stats.DestructorCount;
	if (!Name.empty())
	{
		Stats.Events.push_back("Destructor " + Name);
	}
}

void WTestObject::OnDestroyed()
{
	FTestObjectStats& Stats = FTestObjectStats::Get();
	++Stats.OnDestroyedCount;
	if (!IsValid(this))
	{
		++Stats.InvalidInOnDestroyedCount;
	}

	Super::OnDestroyed();
}

void WTestObject::BeginDestroy()
{
	FTestObjectStats& Stats = FTestObjectStats::Get();
	++Stats.BeginDestroyCount;
	if (WeakOther.IsValid())
	{
		++Stats.ValidWeakOtherInBeginDestroyCount;
	}
	if (!Name.empty())
	{
		Stats.Events.push_back("BeginDestroy " + Name);
	}

	Super::BeginDestroy();
}

WW_IMPLEMENT_CLASS(WTestAbstractObject)

void WTestAbstractObject::RegisterClassMembers(WClass& /*Class*/)
{
}

WTestOwnerObject::WTestOwnerObject()
	: Subobject(NewObject<WTestObject>())
{
}

WW_IMPLEMENT_CLASS(WTestMisbehavingObject)

void WTestMisbehavingObject::RegisterClassMembers(WClass& /*Class*/)
{
}

void WTestMisbehavingObject::CallBeginDestroyDirectly()
{
	BeginDestroy();
}

void WTestMisbehavingObject::BeginDestroy()
{
	switch (Behavior)
	{
		case EBehavior::SkipSuper:
			return;
		case EBehavior::CollectGarbage:
			CollectGarbage();
			break;
		case EBehavior::AddSelfToRoot:
			AddToRoot();
			break;
		case EBehavior::CreateObject:
			NewObject<WTestObject>();
			break;
		default:
			break;
	}

	Super::BeginDestroy();
}

FScopedObjectTest::FScopedObjectTest()
{
	CollectGarbage();
	FTestObjectStats::Get() = FTestObjectStats();
}

FScopedObjectTest::~FScopedObjectTest()
{
	CollectGarbage();
}
