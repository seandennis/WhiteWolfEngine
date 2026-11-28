#pragma once

#include "CoreMinimal.h"

/** Whether a command can be given while a game plays in the editor (FCommandDescription::WhilePlaying). */
enum class ECommandWhilePlaying : uint8
{
	/** The command acts on what is shown, which while a game plays is the game's copy of the scene. */
	Allowed,

	/**
	 * The command would act on what waits unchanged until the game stops: the scene being edited as a whole, its
	 * file, its history, the project, or a material that the scene being edited shares with the game. The editor
	 * refuses it while a game plays.
	 */
	Refused,
};
