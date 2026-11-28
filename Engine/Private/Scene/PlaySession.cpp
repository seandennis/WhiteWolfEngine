#include "Scene/PlaySession.h"

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
	bIsPaused = false;
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
