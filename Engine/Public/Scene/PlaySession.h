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
 * its end, after its components have ended play. The game goes on by itself as real time passes (Advance), unless it
 * is paused; Step plays frames whether it is paused or not.
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

	/** Pauses the game: real time no longer passes for it (Advance). Does nothing if it is paused already. */
	ENGINE_API void Pause();

	/**
	 * Lets a paused game go on, without the part of a frame that was due when it was paused. Does nothing if it is
	 * not paused.
	 */
	ENGINE_API void Resume();

	/**
	 * Plays frames now, whether the game is paused or not, and leaves it paused or not as it was.
	 *
	 * @param FrameCount How many frames to play: at least one, or it is a fatal error.
	 */
	ENGINE_API void Step(int32 FrameCount);

	/**
	 * The most frames that Advance plays at once. What is due beyond them is forgotten, so that a machine that
	 * cannot keep up plays the game slower instead of falling ever further behind.
	 */
	static constexpr int32 MaxFramesPerAdvance = 5;

	/**
	 * Lets real time pass for the game: plays the frames that the time makes due, with what earlier calls left that
	 * was less than a frame, but at most MaxFramesPerAdvance, and forgets what is due beyond them. While the game is
	 * paused nothing is played and no time is kept, and time that is not a number of seconds above zero passes for
	 * nothing.
	 *
	 * @param ElapsedSeconds The real time since the last call.
	 * @return How many frames were played.
	 */
	ENGINE_API int32 Advance(double ElapsedSeconds);

private:
	/** The scene of the game. Weak, because the root set is what keeps it alive; this is how to find it. */
	TWeakObjectPtr<WScene> Scene;

	bool bIsPaused = false;

	/** The part of a frame that real time has made due and Advance has not played: less than one between calls. */
	double PendingFrames = 0.0;
};
