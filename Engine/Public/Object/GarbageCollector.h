#pragma once

#include "CoreMinimal.h"

#include "Object/GarbageCollectorSettings.h"

/**
 * Controls the garbage collector.
 *
 * A collection is incremental: it is a sequence of small steps, and game code may run between them. Tick drives it
 * automatically within a per-frame time budget, so games never need the other functions; they exist for tools and
 * tests. Like the rest of the object system, the collector is used from the main thread only.
 *
 * Between two steps of a collection, as between two frames, the only references that keep an object alive are
 * registered TObjectPtr properties and the root set. A plain pointer held across a step may dangle.
 */
class FGarbageCollector
{
public:
	/**
	 * Advances automatic collection. Call once per frame. Starts a collection when the configured interval has
	 * passed or one was requested, then works on the collection in progress for at most the configured time budget.
	 *
	 * @param DeltaSeconds The time since the previous call.
	 */
	ENGINE_API static void Tick(double DeltaSeconds);

	/** Makes the next Tick start a collection without waiting for the interval, for example after a scene change. */
	ENGINE_API static void RequestCollection();

	/** Returns the settings used by Tick. */
	ENGINE_API static FGarbageCollectorSettings GetSettings();

	/** Replaces the settings used by Tick. Invalid settings are a fatal error. */
	ENGINE_API static void SetSettings(const FGarbageCollectorSettings& Settings);

	/**
	 * Destroys every object that is unreachable at the time of the call, before returning. A collection that is in
	 * progress is finished first.
	 */
	ENGINE_API static void CollectGarbage();

	/** Returns true while a collection has started and not yet finished. */
	ENGINE_API static bool IsCollectionInProgress();

	/** Starts a collection without doing any of its work. No collection may be in progress. */
	ENGINE_API static void StartCollection();

	/**
	 * Does up to MaxWorkUnits steps of the collection in progress. One unit is roughly the work of examining one
	 * object. Does nothing when no collection is in progress.
	 *
	 * @return True if no collection is in progress when the call returns.
	 */
	ENGINE_API static bool AdvanceCollection(int32 MaxWorkUnits);

	/** Returns the number of objects that currently exist, classes included. */
	ENGINE_API static int32 GetObjectCount();

	/**
	 * Runs a full collection and then reports every object that still exists and is not a class, by logging an
	 * error for each. At shutdown, when the scene and every engine system have been released, any such object was
	 * left in the root set by mistake.
	 *
	 * @return The number of objects reported.
	 */
	ENGINE_API static int32 ReportLeakedObjects();
};
