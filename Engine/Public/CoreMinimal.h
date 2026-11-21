#pragma once

// The minimal set of core definitions: fixed-width types, assertions and logging.
//
// Every engine header includes this file first. The headers listed here are the exception: they
// include "Core/CoreTypes.h" directly, because they are part of CoreMinimal.h themselves.
#include "Core/Assert.h"
#include "Core/CoreTypes.h"
#include "Logging/Log.h"
