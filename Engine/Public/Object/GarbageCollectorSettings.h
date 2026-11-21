#pragma once

#include "CoreMinimal.h"

/** Controls when automatic garbage collection runs and how much time it may take from each frame. */
struct FGarbageCollectorSettings
{
	/**
	 * Seconds between the end of one automatic collection and the start of the next. Zero starts a new collection
	 * as soon as the previous one has finished. Must not be negative.
	 */
	double IntervalSeconds = 60.0;

	/**
	 * The time one call to FGarbageCollector::Tick may spend collecting, in seconds. A collection that needs more
	 * is continued by the following calls. Must be greater than zero.
	 */
	double TimeBudgetSeconds = 0.002;
};
