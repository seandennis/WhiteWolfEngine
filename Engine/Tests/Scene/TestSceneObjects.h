#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Component.h"
#include "Scene/EndPlayReason.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <functional>
#include <string>
#include <vector>

class WTestComponent;

/** Records what happens to the test components, so tests can observe their lifecycle. */
struct FTestComponentStats
{
	/**
	 * One entry per lifecycle call, in order: "Attached <Label>", "Detached <Label>", "BeginPlay <Label>",
	 * "Tick <Label> <frame> <DeltaSeconds>" or "EndPlay <Label> <PlayEnded or Destroyed>".
	 */
	std::vector<std::string> Events;

	/** Detached calls during which GetOwner still returned the entity. */
	int32 DetachedWithOwnerCount = 0;

	/**
	 * Called by every test component in BeginPlay, Tick and EndPlay, after the event is recorded, if they are set:
	 * how a test has a component do something in a game. They refer to nothing that a test destroys before the
	 * scenes they act in, because a scene that still plays when it is destroyed ends its game, which calls them.
	 */
	std::function<void(WTestComponent& Component)> OnBeginPlay;
	std::function<void(WTestComponent& Component)> OnTick;
	std::function<void(WTestComponent& Component, EEndPlayReason Reason)> OnEndPlay;

	/** Returns the stats shared by every test component. */
	static FTestComponentStats& Get();
};

/**
 * A component with properties of several kinds, including references to entities and components. It records its
 * lifecycle in FTestComponentStats, play included. Its class does not tick, but records a tick all the same, so that
 * a frame that ticked it would be seen.
 */
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
	void BeginPlay() override;
	void Tick(double DeltaSeconds) override;
	void EndPlay(EEndPlayReason Reason) override;
};

/** A test component that ticks: it counts the frames, moves its entity and records every frame. */
WCLASS()
class WTestPlayComponent : public WTestComponent
{
	WW_DECLARE_CLASS(WTestPlayComponent, WTestComponent)

public:
	WTestPlayComponent();

	/**
	 * The number of frames the component has ticked in. Saved, so that the text of a scene shows how long it
	 * played.
	 */
	WPROPERTY()
	int32 TickCount = 0;

	/** How far the component moves its entity in a second of the game, in metres. */
	WPROPERTY()
	FVector Velocity = FVector(0.0);

protected:
	void Tick(double DeltaSeconds) override;
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
