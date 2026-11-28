#pragma once

#include "CoreMinimal.h"

/**
 * What a command can change of what the editor's history keeps: the scene being edited, and the assets that the
 * editor changes (FEditedAssets).
 *
 * The editor writes the scene and those assets down after every command that can change either, to make the change a
 * step of the history, which takes time in proportion to the size of the scene. A command that only reads, or acts on
 * something else, says so and is spared that. Saying so wrongly is not dangerous: a change such a command makes after
 * all is recorded with the next step, like any change made without a command.
 */
enum class ECommandEffect : uint8
{
	/** The command can change the scene. This is what a command is taken to do unless it says otherwise. */
	ChangesScene,

	/** The command can change assets, as the values of a material, and leaves the scene as it is. */
	ChangesAssets,

	/**
	 * The command changes nothing that the history keeps: it reads, or acts on something else, such as a file or what
	 * is selected.
	 */
	LeavesScene,
};
