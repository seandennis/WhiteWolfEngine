#include "Scene/PlaySession.h"

#include <cmath>

FPlaySession::FPlaySession(WScene& InScene, double FrameSeconds)
	: Scene(&InScene)
{
	// The scene checks what it is asked to play. It is rooted only once its game has begun, so that a game that cannot
	// begin leaves nothing in the root set.
	InScene.BeginPlay(FrameSeconds);
	InScene.AddToRoot();
}

FPlaySession::~FPlaySession()
{
	// Destroying a scene that plays ends its game first, and takes the scene out of the root set.
	if (WScene* const PlayedScene = Scene.Get())
	{
		PlayedScene->Destroy();
	}
}

WScene& FPlaySession::GetScene() const
{
	WScene* const PlayedScene = Scene.Get();
	WW_CHECKF(PlayedScene != nullptr, "The scene of a game was destroyed by something other than the game");
	return *PlayedScene;
}

void FPlaySession::Pause()
{
	bIsPaused = true;
}

void FPlaySession::Resume()
{
	if (bIsPaused)
	{
		// The part of a frame that was due when the game was paused is not owed to it afterwards.
		bIsPaused = false;
		PendingFrames = 0.0;
	}
}

void FPlaySession::Step(int32 FrameCount)
{
	WW_CHECKF(FrameCount >= 1, "A game cannot be played for {} frames: at least one is played", FrameCount);

	WScene& PlayedScene = GetScene();
	for (int32 Frame = 0; Frame < FrameCount; ++Frame)
	{
		PlayedScene.StepFrame();
	}
}

int32 FPlaySession::Advance(double ElapsedSeconds)
{
	const bool bIsTime = std::isfinite(ElapsedSeconds) && ElapsedSeconds > 0.0;
	if (bIsPaused || !bIsTime)
	{
		return 0;
	}

	// Time is kept in frames, so that time that is exactly a frame long is exactly one frame, however long the game
	// goes on: seconds that were added up would drift from the frames.
	PendingFrames += ElapsedSeconds / GetScene().GetFrameSeconds();
	const double DueFrames = std::floor(PendingFrames);
	if (DueFrames > MaxFramesPerAdvance)
	{
		PendingFrames = 0.0;
		Step(MaxFramesPerAdvance);
		return MaxFramesPerAdvance;
	}

	PendingFrames -= DueFrames;
	const int32 FrameCount = static_cast<int32>(DueFrames);
	if (FrameCount > 0)
	{
		Step(FrameCount);
	}
	return FrameCount;
}
