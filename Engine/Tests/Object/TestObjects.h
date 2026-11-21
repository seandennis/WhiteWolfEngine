#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"

#include <string>
#include <vector>

/** Counts and records what happens to the test objects, so tests can observe the garbage collector. */
struct FTestObjectStats
{
	/** Objects whose constructor has run and whose destructor has not. */
	int32 AliveCount = 0;

	/** Total BeginDestroy calls. */
	int32 BeginDestroyCount = 0;

	/** Total destructor calls. */
	int32 DestructorCount = 0;

	/** BeginDestroy calls that found the object's WeakOther reference still valid. */
	int32 ValidWeakOtherInBeginDestroyCount = 0;

	/** One entry per lifetime event of a named object, in order: "BeginDestroy <Name>" or "Destructor <Name>". */
	std::vector<std::string> Events;

	/** Returns the stats shared by every test object. */
	static FTestObjectStats& Get();
};

/** An object with every kind of reference the collector must follow, and one it must not. */
WCLASS()
class WTestObject : public WObject
{
	WW_DECLARE_CLASS(WTestObject, WObject)

public:
	WTestObject();
	~WTestObject() override;

	/** Reflected: keeps its target alive. */
	WPROPERTY()
	TObjectPtr<WTestObject> Other;

	/** Reflected: keeps every element alive. */
	WPROPERTY()
	std::vector<TObjectPtr<WTestObject>> Children;

	/** Reflected, but weak: does not keep its target alive. */
	WPROPERTY()
	TWeakObjectPtr<WTestObject> WeakOther;

	/** Not reflected: does not keep its target alive. */
	TObjectPtr<WTestObject> Unreflected;

	/** When set, lifetime events of this object are recorded in FTestObjectStats::Events under this name. */
	std::string Name;

protected:
	void BeginDestroy() override;
};

/** A derived class, to test inheritance of classes and of reference properties. */
WCLASS()
class WTestDerivedObject : public WTestObject
{
	WW_DECLARE_CLASS(WTestDerivedObject, WTestObject)

public:
	/** Reflected, and typed as the base of all objects. */
	WPROPERTY()
	TObjectPtr<WObject> Extra;
};

/** A class that cannot be instantiated. It is registered by hand, without WCLASS(), which must keep working. */
class WTestAbstractObject : public WObject
{
	WW_DECLARE_CLASS(WTestAbstractObject, WObject)

public:
	virtual void DoSomething() = 0;
};

/** Creates another object in its constructor and keeps it in a reflected property. */
WCLASS()
class WTestOwnerObject : public WObject
{
	WW_DECLARE_CLASS(WTestOwnerObject, WObject)

public:
	WTestOwnerObject();

	WPROPERTY()
	TObjectPtr<WTestObject> Subobject;
};

/** Misbehaves in BeginDestroy in a way chosen by the test. Registered by hand, like WTestAbstractObject. */
class WTestMisbehavingObject : public WObject
{
	WW_DECLARE_CLASS(WTestMisbehavingObject, WObject)

public:
	enum class EBehavior : uint8
	{
		/** Does not call Super::BeginDestroy. */
		SkipSuper,

		/** Calls CollectGarbage from BeginDestroy. */
		CollectGarbage,

		/** Adds itself to the root set from BeginDestroy. */
		AddSelfToRoot,

		/** Creates a new WTestObject from BeginDestroy. */
		CreateObject,
	};

	EBehavior Behavior = EBehavior::SkipSuper;

	/** Calls BeginDestroy directly, which only the collector may do. */
	void CallBeginDestroyDirectly();

protected:
	void BeginDestroy() override;
};

/**
 * Isolates a test that creates objects: resets the stats when created and, when destroyed, collects garbage so that
 * nothing the test created survives into the next test.
 */
class FScopedObjectTest
{
public:
	FScopedObjectTest();
	~FScopedObjectTest();

	FScopedObjectTest(const FScopedObjectTest&) = delete;
	FScopedObjectTest& operator=(const FScopedObjectTest&) = delete;
};
