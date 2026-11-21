#include "Object/TestObjects.h"

#include "Object/ObjectGlobals.h"
#include "Reflection/PropertyFactory.h"

FTestObjectStats& FTestObjectStats::Get()
{
	static FTestObjectStats Stats;
	return Stats;
}

WW_IMPLEMENT_CLASS(WTestObject)

void WTestObject::RegisterClassMembers(WClass& Class)
{
	Class.AddProperty(MakeProperty<&WTestObject::Other>("Other"));
	Class.AddProperty(MakeProperty<&WTestObject::Children>("Children"));
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

void WTestObject::BeginDestroy()
{
	FTestObjectStats& Stats = FTestObjectStats::Get();
	++Stats.BeginDestroyCount;
	if (!Name.empty())
	{
		Stats.Events.push_back("BeginDestroy " + Name);
	}

	Super::BeginDestroy();
}

WW_IMPLEMENT_CLASS(WTestDerivedObject)

void WTestDerivedObject::RegisterClassMembers(WClass& Class)
{
	Class.AddProperty(MakeProperty<&WTestDerivedObject::Extra>("Extra"));
}

WW_IMPLEMENT_CLASS(WTestAbstractObject)

void WTestAbstractObject::RegisterClassMembers(WClass& /*Class*/)
{
}

WW_IMPLEMENT_CLASS(WTestOwnerObject)

void WTestOwnerObject::RegisterClassMembers(WClass& Class)
{
	Class.AddProperty(MakeProperty<&WTestOwnerObject::Subobject>("Subobject"));
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
