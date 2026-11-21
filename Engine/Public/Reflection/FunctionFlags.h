#pragma once

#include "CoreMinimal.h"

#include "Core/EnumClassFlags.h"

/** Options of a reflected function. The Blueprint ones are set with specifiers: WFUNCTION(BlueprintCallable). */
enum class EFunctionFlags : uint8
{
	None = 0,

	/** Blueprints can call the function as a node with execution pins. */
	BlueprintCallable = 1 << 0,

	/** Blueprints can call the function as a node without execution pins; it must not change anything. */
	BlueprintPure = 1 << 1,

	/** The function is a const member function. Set automatically. */
	Const = 1 << 2,
};

WW_ENUM_CLASS_FLAGS(EFunctionFlags)
