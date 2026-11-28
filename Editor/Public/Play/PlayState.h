#pragma once

#include "CoreMinimal.h"

/** Whether a game plays in the editor (FEditor::GetPlayState). */
enum class EPlayState : uint8
{
	/** No game: the scene being edited is what is shown and changed. */
	Stopped,

	/** A game plays in a copy of the scene being edited, which is what is shown and changed until it stops. */
	Playing,

	/** A game exists, as while it plays, but does not go on by itself; it can still be played a frame at a time. */
	Paused,
};
