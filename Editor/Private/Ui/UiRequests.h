#pragma once

#include "CoreMinimal.h"

#include <optional>

/**
 * What one part of the user interface asks of another. Whoever is asked looks at the request when it is next laid
 * out, and takes it away.
 */
struct FUiRequests
{
	/** The entity whose row in the outliner is to turn into a text field for its name. */
	std::optional<int64> EntityToRename;
};
