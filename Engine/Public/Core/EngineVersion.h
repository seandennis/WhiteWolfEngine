#pragma once

#include "CoreMinimal.h"

#include "Core/Version.h"

/** Identifies the version of the engine that a binary was built from. */
class FEngineVersion
{
public:
	/** Returns the version of the running engine. */
	ENGINE_API static FVersion Current();
};
