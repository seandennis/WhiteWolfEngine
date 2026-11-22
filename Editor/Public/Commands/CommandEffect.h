#pragma once

#include "CoreMinimal.h"

/**
 * Whether a command can change the scene being edited.
 *
 * The editor writes the scene down after every command that can, to make the change a step of the history, which
 * takes time in proportion to the size of the scene. A command that only reads, or acts on something other than the
 * scene, says so and is spared that. Saying so wrongly is not dangerous: a change such a command makes after all is
 * recorded with the next step, like any change made without a command.
 */
enum class ECommandEffect : uint8
{
	/** The command can change the scene. This is what a command is taken to do unless it says otherwise. */
	ChangesScene,

	/** The command leaves the scene as it is: it reads from it, or acts on something else, such as a file. */
	LeavesScene,
};
