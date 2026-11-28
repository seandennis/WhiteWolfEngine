#include "Scene/PlaySession.h"

#include "Object/GarbageCollector.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

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
