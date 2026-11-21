#pragma once

#include "CoreMinimal.h"

/** What happened to a key or mouse button. */
enum class EInputEvent : uint8
{
	/** It went down. */
	Pressed,

	/** It came up. */
	Released,
};
