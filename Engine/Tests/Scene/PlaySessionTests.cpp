#include "Scene/PlaySession.h"

#include "Object/GarbageCollector.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <limits>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** Makes a scene that nothing keeps alive yet, with an entity that has a component that ticks, called Ticking. */
	WScene* MakeGameScene()
	{
		WScene* const Scene = NewObject<WScene>();
		WEntity* const Entity = Scene->CreateEntity("Entity");
		REQUIRE(Entity != nullptr);
		Entity->AddComponent<WTestPlayComponent>()->Label = "Ticking";
		return Scene;
	}

	/** Plays a game in a scene and ends it at once: what a test does that expects a game not to begin. */
	void PlayAndEnd(WScene& Scene, double FrameSeconds = WScene::DefaultFrameSeconds)
	{
		const FPlaySession Session(Scene, FrameSeconds);
	}
}

TEST_SUITE("FPlaySession")
{
	TEST_CASE("A session begins the game of its scene at frame 0, and keeps the scene alive")
	{
		const FScopedSceneTest SceneTest;
		WScene* const Scene = MakeGameScene();

		const FPlaySession Session(*Scene);

		CHECK(&Session.GetScene() == Scene);
		CHECK(Scene->IsPlaying());
		CHECK(Scene->GetPlayedFrameCount() == 0);
		CHECK(Scene->GetFrameSeconds() == WScene::DefaultFrameSeconds);
		CHECK_FALSE(Session.IsPaused());
		CHECK(FTestComponentStats::Get().Events.back() == "BeginPlay Ticking");

		// Nothing but the session keeps the scene alive. Weak, because a collected scene is no object to ask.
		const TWeakObjectPtr<WScene> Kept = Scene;
		CollectGarbage();
		CHECK(Kept.IsValid());
	}

	TEST_CASE("A session plays in frames of the length it is given")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*MakeGameScene(), 0.25);

		Session.Step(2);

		CHECK(Session.GetScene().GetFrameSeconds() == 0.25);
		CHECK(Session.GetScene().GetPlayedSeconds() == 0.5);
	}

	TEST_CASE("Step plays frames whether the game is paused or not, and leaves it as it was")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*MakeGameScene(), 0.25);

		Session.Step(3);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 3);
		CHECK_FALSE(Session.IsPaused());

		Session.Pause();
		Session.Step(1);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 4);
		CHECK(Session.IsPaused());
		CHECK(FTestComponentStats::Get().Events.back() == "Tick Ticking 4 0.25");
	}

	TEST_CASE("Pause and Resume change only whether the game is paused")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*MakeGameScene());

		Session.Pause();
		CHECK(Session.IsPaused());
		Session.Pause();
		CHECK(Session.IsPaused());
		Session.Resume();
		CHECK_FALSE(Session.IsPaused());
		Session.Resume();
		CHECK_FALSE(Session.IsPaused());

		CHECK(Session.GetScene().IsPlaying());
		CHECK(Session.GetScene().GetPlayedFrameCount() == 0);
	}

	TEST_CASE("The end of a session ends the game and destroys the scene, which a collection then takes")
	{
		const FScopedSceneTest SceneTest;
		const int32 ObjectCountBefore = FGarbageCollector::GetObjectCount();
		TWeakObjectPtr<WScene> Scene;
		{
			FPlaySession Session(*MakeGameScene());
			Scene = &Session.GetScene();
			Session.Step(1);
			FTestComponentStats::Get().Events.clear();
		}

		CHECK_FALSE(Scene.IsValid());
		CHECK(FTestComponentStats::Get().Events ==
			std::vector<std::string>{"EndPlay Ticking PlayEnded", "Detached Ticking"});
		CollectGarbage();
		CHECK(FGarbageCollector::GetObjectCount() == ObjectCountBefore);
	}

	TEST_CASE("A scene destroyed by something else is a fatal error when asked for, and the session ends quietly")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		{
			FPlaySession Session(*MakeGameScene());
			Session.GetScene().Destroy();

			CHECK_THROWS_AS(Session.GetScene(), FFatalErrorException);
			CHECK_THROWS_AS(Session.Step(1), FFatalErrorException);
			FTestComponentStats::Get().Events.clear();
		}

		CHECK(FTestComponentStats::Get().Events.empty());
	}

	TEST_CASE("Real time of exactly a frame plays exactly a frame, for an hour of the game")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*NewObject<WScene>());

		int32 PlayedFrames = 0;
		for (int32 Call = 0; Call < 216000; ++Call)
		{
			PlayedFrames += Session.Advance(WScene::DefaultFrameSeconds);
		}

		CHECK(PlayedFrames == 216000);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 216000);
	}

	TEST_CASE("Real time that is not a whole number of frames is kept until it makes one")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*NewObject<WScene>(), 0.25);

		CHECK(Session.Advance(0.125) == 0);
		CHECK(Session.Advance(0.125) == 1);
		CHECK(Session.Advance(0.125) == 0);
		CHECK(Session.Advance(0.125) == 1);
		CHECK(Session.Advance(0.625) == 2);
		CHECK(Session.Advance(0.625) == 3);
		CHECK(Session.Advance(0.625) == 2);
		CHECK(Session.Advance(0.625) == 3);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 12);
	}

	TEST_CASE("Advance plays at most five frames at once, and forgets what is due beyond them")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*NewObject<WScene>(), 0.25);

		// Five frames and a half: the five are played as they are due, and the half is kept.
		CHECK(Session.Advance(1.375) == 5);
		CHECK(Session.Advance(0.125) == 1);
		// Six are one too many: five are played, and nothing of the time is owed afterwards.
		CHECK(Session.Advance(1.5) == FPlaySession::MaxFramesPerAdvance);
		CHECK(Session.Advance(0.125) == 0);
		CHECK(Session.Advance(0.125) == 1);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 12);
	}

	TEST_CASE("Time that is not a number of seconds above zero passes for nothing")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*NewObject<WScene>(), 0.25);
		CHECK(Session.Advance(0.125) == 0);

		for (const double Time : {0.0,
				 -0.25,
				 std::numeric_limits<double>::quiet_NaN(),
				 std::numeric_limits<double>::infinity(),
				 -std::numeric_limits<double>::infinity()})
		{
			CAPTURE(Time);
			CHECK(Session.Advance(Time) == 0);
		}

		// The half frame from before is still due, no more and no less.
		CHECK(Session.Advance(0.125) == 1);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 1);
	}

	TEST_CASE("Nothing plays while the game is paused, and resuming forgets the part of a frame that was due")
	{
		const FScopedSceneTest SceneTest;
		FPlaySession Session(*NewObject<WScene>(), 0.25);
		CHECK(Session.Advance(0.125) == 0);

		Session.Pause();
		CHECK(Session.Advance(1.0) == 0);
		CHECK(Session.GetScene().GetPlayedFrameCount() == 0);

		Session.Resume();
		CHECK(Session.Advance(0.125) == 0);
		CHECK(Session.Advance(0.125) == 1);

		// Resuming a game that is not paused keeps what is due.
		CHECK(Session.Advance(0.125) == 0);
		Session.Resume();
		CHECK(Session.Advance(0.125) == 1);
	}

	TEST_CASE("Playing fewer than one frame is a fatal error")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		FPlaySession Session(*MakeGameScene());

		CHECK_THROWS_AS(Session.Step(0), FFatalErrorException);
		CHECK_THROWS_AS(Session.Step(-1), FFatalErrorException);

		CHECK(Session.GetScene().GetPlayedFrameCount() == 0);
	}

	TEST_CASE("A game cannot begin in a scene that plays already, a destroyed scene, or in frames of no length")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;

		SUBCASE("A scene that plays already")
		{
			const FScopedTestScene Scene;
			Scene->BeginPlay();

			CHECK_THROWS_AS(PlayAndEnd(*Scene.Get()), FFatalErrorException);

			CHECK(Scene->IsPlaying());
			Scene->EndPlay();
		}
		SUBCASE("A destroyed scene")
		{
			WScene* const Scene = MakeGameScene();
			Scene->Destroy();

			CHECK_THROWS_AS(PlayAndEnd(*Scene), FFatalErrorException);

			CHECK_FALSE(Scene->IsRooted());
		}
		SUBCASE("Frames of no length")
		{
			WScene* const Scene = MakeGameScene();

			CHECK_THROWS_AS(PlayAndEnd(*Scene, 0.0), FFatalErrorException);

			CHECK_FALSE(Scene->IsPlaying());
			CHECK_FALSE(Scene->IsRooted());
		}
	}
}
