#pragma once

#include "CoreMinimal.h"

#include "Core/EnumClassFlags.h"

/** Options of a reflected property. The first three are set with specifiers: WPROPERTY(Transient). */
enum class EPropertyFlags : uint8
{
	None = 0,

	/** The value is not saved with the object and not loaded; it is worked out again at run time. */
	Transient = 1 << 0,

	/** Blueprints can read the value but not change it. */
	BlueprintReadOnly = 1 << 1,

	/** Blueprints can read and change the value. */
	BlueprintReadWrite = 1 << 2,

	/** The property is a parameter through which a reflected function returns a value: a non-const reference. */
	OutParameter = 1 << 3,
};

WW_ENUM_CLASS_FLAGS(EPropertyFlags)
