#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"
#include "Scene/Scene.h"

/**
 * A game that is being played: a scene that plays (WScene::BeginPlay) from the session's beginning to its end, and
 * whether the game is paused. What the editor plays in a copy of the scene being edited, and what a game that is
 * exported plays in the scene it starts with.
 *
 * The scene is the game's: the session keeps it alive, in the root set, while the game goes on, and destroys it at
 * its end, after its components have ended play. Pausing keeps the game from going on by itself; Step plays frames
 * whether it is paused or not.
 *
 * Create and destroy a session on the main thread, and never from inside a component's BeginPlay, Tick or EndPlay.
 */
class FPlaySession
{
public:
	/**
	 * Begins the game of a scene: every component of the scene begins play, and the scene is kept alive from now on.
	 *
	 * A fatal error if the scene has been destroyed or plays already, or if FrameSeconds is not a number of seconds
	 * above zero.
	 *
	 * @param InScene The scene to play, which becomes the game's.
	 * @param FrameSeconds The game time that a frame stands for.
	 */
	ENGINE_API explicit FPlaySession(WScene& InScene, double FrameSeconds = WScene::DefaultFrameSeconds);

	/**
	 * Ends the game, every component that began play told that it ended, and destroys the scene. Does nothing to a
	 * scene that something else destroyed already.
	 */
	ENGINE_API ~FPlaySession();

	FPlaySession(const FPlaySession&) = delete;
	FPlaySession& operator=(const FPlaySession&) = delete;

	/** Returns the scene of the game. A fatal error if something other than the session has destroyed it. */
	ENGINE_API WScene& GetScene() const;

	/** Returns true while the game is paused. */
	bool IsPaused() const
	{
		return bIsPaused;
	}

	/** Pauses the game. Does nothing if it is paused already. */
	ENGINE_API void Pause();

	/** Lets a paused game go on. Does nothing if it is not paused. */
	ENGINE_API void Resume();

	/**
	 * Plays frames now, whether the game is paused or not, and leaves it paused or not as it was.
	 *
	 * @param FrameCount How many frames to play: at least one, or it is a fatal error.
	 */
	ENGINE_API void Step(int32 FrameCount);

private:
	/** The scene of the game. Weak, because the root set is what keeps it alive; this is how to find it. */
	TWeakObjectPtr<WScene> Scene;

	bool bIsPaused = false;
};
