#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <string>
#include <vector>

/** Records what happens to the test components, so tests can observe their lifecycle. */
struct FTestComponentStats
{
	/** One entry per lifecycle call, in order: "Attached <Label>" or "Detached <Label>". */
	std::vector<std::string> Events;

	/** Detached calls during which GetOwner still returned the entity. */
	int32 DetachedWithOwnerCount = 0;

	/** Returns the stats shared by every test component. */
	static FTestComponentStats& Get();
};

/** A component with properties of several kinds, including references to entities and components. */
WCLASS()
class WTestComponent : public WComponent
{
	WW_DECLARE_CLASS(WTestComponent, WComponent)

public:
	WPROPERTY()
	int32 Health = 100;

	WPROPERTY(Category = "Text")
	std::string Label = "Unnamed";

	WPROPERTY()
	FVector Target = FVector(0.0);

	WPROPERTY()
	TObjectPtr<WEntity> TargetEntity;

	WPROPERTY()
	TWeakObjectPtr<WTestComponent> Partner;

	WPROPERTY()
	std::vector<TObjectPtr<WEntity>> Waypoints;

	/** Not saved. */
	WPROPERTY(Transient)
	float CachedDistance = 0.0f;

protected:
	void OnAttached() override;
	void OnDetached() override;
};

/** A second, unrelated component class. */
WCLASS()
class WTestOtherComponent : public WComponent
{
	WW_DECLARE_CLASS(WTestOtherComponent, WComponent)

public:
	WPROPERTY()
	bool bEnabled = true;
};

/** A component class derived from another one. */
WCLASS()
class WTestDerivedComponent : public WTestComponent
{
	WW_DECLARE_CLASS(WTestDerivedComponent, WTestComponent)

public:
	WPROPERTY()
	double Range = 1.5;
};

/** A component class that cannot be created. */
WCLASS()
class WTestAbstractComponent : public WComponent
{
	WW_DECLARE_CLASS(WTestAbstractComponent, WComponent)

public:
	virtual void DoSomething() = 0;
};

/**
 * A scene that lives as long as this object: it is created in the root set and destroyed, with everything in it,
 * at the end of the scope.
 */
class FScopedTestScene
{
public:
	FScopedTestScene();
	~FScopedTestScene();

	FScopedTestScene(const FScopedTestScene&) = delete;
	FScopedTestScene& operator=(const FScopedTestScene&) = delete;

	WScene* Get() const
	{
		return Scene;
	}

	WScene* operator->() const
	{
		return Scene;
	}

private:
	WScene* Scene;
};

/** Isolates a test that creates scenes: collects garbage before and after it, and resets the component stats. */
class FScopedSceneTest
{
public:
	FScopedSceneTest();
	~FScopedSceneTest();

	FScopedSceneTest(const FScopedSceneTest&) = delete;
	FScopedSceneTest& operator=(const FScopedSceneTest&) = delete;
};
