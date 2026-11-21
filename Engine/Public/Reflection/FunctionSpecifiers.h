#pragma once

#include "CoreMinimal.h"

#include "Reflection/FunctionFlags.h"

#include <string>

/** What the specifiers of a WFUNCTION() say about the function: WFUNCTION(BlueprintCallable, Category = "Audio"). */
struct FFunctionSpecifiers
{
	EFunctionFlags Flags = EFunctionFlags::None;

	/** The group the function is listed under in the Blueprint node menu. Empty for the default group. */
	std::string Category;
};
