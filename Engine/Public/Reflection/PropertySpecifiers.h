#pragma once

#include "CoreMinimal.h"

#include "Reflection/PropertyFlags.h"

#include <string>

/** What the specifiers of a WPROPERTY() say about the property: WPROPERTY(Transient, Category = "Rendering"). */
struct FPropertySpecifiers
{
	EPropertyFlags Flags = EPropertyFlags::None;

	/** The group the property is shown under in the editor's Details panel. Empty for the default group. */
	std::string Category;
};
