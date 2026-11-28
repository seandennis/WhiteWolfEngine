#include "Scene/Scene.h"

#include "Logging/LogMessage.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Component.h"
#include "Scene/EndPlayReason.h"
#include "Scene/Entity.h"
#include "Scene/SceneSerializer.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/** The length of a frame in most of these tests: a quarter of a second, in which every time is exact. */
	constexpr double QuarterSecond = 0.25;

	/** What the test components recorded, in order (FTestComponentStats::Events). */
	using FEvents = std::vector<std::string>;

	/** Adds a test component of the given class to an entity, and gives it a label after it was attached. */
	template <typename ComponentType>
	ComponentType* AddLabelled(WEntity& Entity, std::string_view Label)
	{
		ComponentType* const Component = Entity.AddComponent<ComponentType>();
		REQUIRE(Component != nullptr);
		Component->Label = Label;
		return Component;
	}

	/** Returns what the test components recorded since the last call, and forgets it. */
	FEvents TakeEvents()
	{
		return std::exchange(FTestComponentStats::Get().Events, {});
	}

	/** Returns true if the last message that was logged mentions Text: the message of a fatal error a test caught. */
	bool LastMessageMentions(const FScopedLogCapture& Capture, std::string_view Text)
	{
		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		return !Messages.empty() && Messages.back().Text.find(Text) != std::string::npos;
	}

	/** One of the functions of a scene that play its game, as a component might call it. */
	struct FPlayCall
	{
		const char* Name;
		void (*Call)(WScene& Played);
	};

	constexpr FPlayCall PlayCalls[] = {
		{"BeginPlay",
			[](WScene& Played)
			{
				Played.BeginPlay(QuarterSecond);
			}},
		{"StepFrame",
			[](WScene& Played)
			{
				Played.StepFrame();
			}},
		{"EndPlay",
			[](WScene& Played)
			{
				Played.EndPlay();
			}},
	};

	/** What a fatal error says when a component calls one of the play functions of its scene. */
	constexpr std::string_view FromInsideMessage = "from inside a component's BeginPlay, Tick or EndPlay";
}

TEST_SUITE("WScene play")
{
	TEST_CASE("A scene that has not begun play does not play, and has played no frame")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		const WTestPlayComponent* const Ticking = Entity->AddComponent<WTestPlayComponent>();

		CHECK_FALSE(Scene->IsPlaying());
		CHECK(Scene->GetPlayedFrameCount() == 0);
		CHECK(Scene->GetPlayedSeconds() == 0.0);
		CHECK(Scene->GetFrameSeconds() == WScene::DefaultFrameSeconds);
		CHECK_FALSE(Ticking->HasBegunPlay());
		CHECK(TakeEvents() == FEvents{"Attached Unnamed"});
	}

	TEST_CASE("BeginPlay begins every component once, after it was attached, in the order of the scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		// Made in another order than that of the scene, which is First, its child, and then Second.
		WEntity* const First = Scene->CreateEntity("First");
		WEntity* const Second = Scene->CreateEntity("Second");
		WEntity* const Child = Scene->CreateEntity("Child", First);
		AddLabelled<WTestComponent>(*Second, "S");
		AddLabelled<WTestComponent>(*Child, "C");
		AddLabelled<WTestComponent>(*First, "F1");
		AddLabelled<WTestPlayComponent>(*First, "F2");

		Scene->BeginPlay(QuarterSecond);

		CHECK(Scene->IsPlaying());
		CHECK(Scene->GetFrameSeconds() == QuarterSecond);
		CHECK(Scene->GetPlayedFrameCount() == 0);
		CHECK(TakeEvents() ==
			FEvents{"Attached Unnamed",
				"Attached Unnamed",
				"Attached Unnamed",
				"Attached Unnamed",
				"BeginPlay F1",
				"BeginPlay F2",
				"BeginPlay C",
				"BeginPlay S"});
		for (const WComponent* const Each : Scene->GetComponents<WComponent>())
		{
			CHECK(Each->HasBegunPlay());
		}

		// A frame begins nothing again.
		Scene->StepFrame();
		CHECK(TakeEvents() == FEvents{"Tick F2 1 0.25"});
		Scene->EndPlay();
	}

	TEST_CASE("Only components that tick are ticked, in the order of the scene, with the length of a frame")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const First = Scene->CreateEntity("First");
		WEntity* const Second = Scene->CreateEntity("Second");
		WEntity* const Child = Scene->CreateEntity("Child", First);
		const WTestPlayComponent* const Late = AddLabelled<WTestPlayComponent>(*Second, "Late");
		AddLabelled<WTestComponent>(*First, "Still");
		const WTestPlayComponent* const Early = AddLabelled<WTestPlayComponent>(*Child, "Early");
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Scene->StepFrame();
		Scene->StepFrame();

		CHECK(
			TakeEvents() == FEvents{"Tick Early 1 0.25", "Tick Late 1 0.25", "Tick Early 2 0.25", "Tick Late 2 0.25"});
		CHECK(Early->TickCount == 2);
		CHECK(Late->TickCount == 2);
		Scene->EndPlay();
	}

	TEST_CASE("The game time is the number of frames times the length of a frame, exactly")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		Scene->BeginPlay(QuarterSecond);
		CHECK(Scene->GetPlayedSeconds() == 0.0);
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Scene->StepFrame();
		}

		CHECK(Scene->GetPlayedFrameCount() == 3);
		CHECK(Scene->GetPlayedSeconds() == 0.75);

		// The count and the time stay as they were when the game ended.
		Scene->EndPlay();
		CHECK(Scene->GetPlayedFrameCount() == 3);
		CHECK(Scene->GetPlayedSeconds() == 0.75);

		// Sixty frames of a sixtieth of a second are exactly one second, which sixty sixtieths added up are not.
		Scene->BeginPlay();
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			Scene->StepFrame();
		}
		CHECK(Scene->GetPlayedSeconds() == 1.0);
		Scene->EndPlay();
	}

	TEST_CASE("A component added during a game begins at the next frame, with its properties, before anything ticks")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const First = Scene->CreateEntity("First");
		WEntity* const Second = Scene->CreateEntity("Second");
		AddLabelled<WTestPlayComponent>(*First, "Ticking");
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		const WTestComponent* const Added = AddLabelled<WTestComponent>(*Second, "Added");

		CHECK_FALSE(Added->HasBegunPlay());
		Scene->StepFrame();

		CHECK(Added->HasBegunPlay());
		// It was attached before it was given its label, and began with it, before the component of the entity that
		// comes first in the scene ticked.
		CHECK(TakeEvents() == FEvents{"Attached Unnamed", "BeginPlay Added", "Tick Ticking 1 0.25"});
		Scene->EndPlay();
	}

	TEST_CASE("A component added during a frame begins in the next one")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestPlayComponent>(*Entity, "Spawner");
		FTestComponentStats::Get().OnTick = [](WTestComponent& Component)
		{
			if (Component.GetScene()->GetPlayedFrameCount() == 1)
			{
				Component.GetOwner()->AddComponent<WTestComponent>()->Label = "Spawned";
			}
		};
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Scene->StepFrame();
		CHECK(TakeEvents() == FEvents{"Tick Spawner 1 0.25", "Attached Unnamed"});

		Scene->StepFrame();
		CHECK(TakeEvents() == FEvents{"BeginPlay Spawned", "Tick Spawner 2 0.25"});
		Scene->EndPlay();
	}

	TEST_CASE("A component added and removed between two frames neither begins nor ends play")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestPlayComponent>(*Entity, "Ticking");
		Scene->BeginPlay(QuarterSecond);
		Scene->StepFrame();
		TakeEvents();

		WTestComponent* const Passing = AddLabelled<WTestComponent>(*Entity, "Passing");
		CHECK(Entity->RemoveComponent(Passing));
		Scene->StepFrame();

		CHECK(TakeEvents() == FEvents{"Attached Unnamed", "Detached Passing", "Tick Ticking 2 0.25"});
		Scene->EndPlay();
		CHECK(TakeEvents() == FEvents{"EndPlay Ticking PlayEnded"});
	}

	TEST_CASE("A component removed during a game ends play before it is detached, and ticks no more")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		WTestPlayComponent* const Removed = AddLabelled<WTestPlayComponent>(*Entity, "Removed");
		AddLabelled<WTestPlayComponent>(*Entity, "Kept");
		// In EndPlay, the component still has its entity and scene.
		FTestComponentStats::Get().OnEndPlay = [](WTestComponent& Component, EEndPlayReason)
		{
			const bool bHasPlace = Component.GetOwner() != nullptr && Component.GetScene() != nullptr;
			FTestComponentStats::Get().Events.push_back(bHasPlace ? "In place" : "Out of place");
		};
		Scene->BeginPlay(QuarterSecond);
		Scene->StepFrame();
		TakeEvents();

		CHECK(Entity->RemoveComponent(Removed));

		CHECK_FALSE(Removed->HasBegunPlay());
		CHECK(TakeEvents() == FEvents{"EndPlay Removed Destroyed", "In place", "Detached Removed"});
		Scene->StepFrame();
		CHECK(TakeEvents() == FEvents{"Tick Kept 2 0.25"});
		Scene->EndPlay();
		CHECK(TakeEvents() == FEvents{"EndPlay Kept PlayEnded", "In place"});
	}

	TEST_CASE("Destroying an entity during a game ends play for its components and those of its descendants")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Parent = Scene->CreateEntity("Parent");
		WEntity* const Child = Scene->CreateEntity("Child", Parent);
		WEntity* const Other = Scene->CreateEntity("Other");
		AddLabelled<WTestComponent>(*Parent, "P");
		AddLabelled<WTestComponent>(*Child, "C");
		AddLabelled<WTestComponent>(*Other, "O");
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Parent->Destroy();

		CHECK(TakeEvents() == FEvents{"EndPlay P Destroyed", "Detached P", "EndPlay C Destroyed", "Detached C"});
		Scene->EndPlay();
		CHECK(TakeEvents() == FEvents{"EndPlay O PlayEnded"});
	}

	TEST_CASE("A component that destroys its own entity in its Tick ends play, and the frame goes on for the others")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Doomed = Scene->CreateEntity("Doomed");
		WEntity* const Survivor = Scene->CreateEntity("Survivor");
		AddLabelled<WTestPlayComponent>(*Doomed, "Doomed");
		AddLabelled<WTestPlayComponent>(*Doomed, "Beside");
		AddLabelled<WTestPlayComponent>(*Survivor, "Survivor");
		FTestComponentStats::Get().OnTick = [](WTestComponent& Component)
		{
			if (Component.Label == "Doomed")
			{
				Component.GetOwner()->Destroy();
			}
		};
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Scene->StepFrame();

		CHECK_FALSE(IsValid(Doomed));
		// The component beside it on the entity ends play without having ticked in that frame.
		CHECK(TakeEvents() ==
			FEvents{"Tick Doomed 1 0.25",
				"EndPlay Doomed Destroyed",
				"Detached Doomed",
				"EndPlay Beside Destroyed",
				"Detached Beside",
				"Tick Survivor 1 0.25"});
		Scene->StepFrame();
		CHECK(TakeEvents() == FEvents{"Tick Survivor 2 0.25"});
		Scene->EndPlay();
	}

	TEST_CASE("An entity made during a frame begins play in the next one")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		AddLabelled<WTestPlayComponent>(*Scene->CreateEntity("Maker"), "Maker");
		FTestComponentStats::Get().OnTick = [](WTestComponent& Component)
		{
			if (Component.GetScene()->GetPlayedFrameCount() == 1)
			{
				Component.GetScene()->CreateEntity("Made")->AddComponent<WTestComponent>()->Label = "Made";
			}
		};
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Scene->StepFrame();

		CHECK(TakeEvents() == FEvents{"Tick Maker 1 0.25", "Attached Unnamed"});
		const WEntity* const Made = Scene->FindEntityByName("Made");
		REQUIRE(Made != nullptr);
		REQUIRE(Made->GetComponents().size() == 1);
		CHECK_FALSE(Made->GetComponents()[0]->HasBegunPlay());

		Scene->StepFrame();

		CHECK(Made->GetComponents()[0]->HasBegunPlay());
		CHECK(TakeEvents() == FEvents{"BeginPlay Made", "Tick Maker 2 0.25"});
		Scene->EndPlay();
	}

	TEST_CASE("A component that destroys itself in BeginPlay is told EndPlay once")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestComponent>(*Entity, "Fleeting");
		AddLabelled<WTestComponent>(*Entity, "Staying");
		FTestComponentStats::Get().OnBeginPlay = [](WTestComponent& Component)
		{
			if (Component.Label == "Fleeting")
			{
				Component.Destroy();
			}
		};
		TakeEvents();

		Scene->BeginPlay(QuarterSecond);

		CHECK(TakeEvents() ==
			FEvents{"BeginPlay Fleeting", "EndPlay Fleeting Destroyed", "Detached Fleeting", "BeginPlay Staying"});
		Scene->EndPlay();
		CHECK(TakeEvents() == FEvents{"EndPlay Staying PlayEnded"});
	}

	TEST_CASE("EndPlay ends every component that began play, in the reverse of the order of the scene")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const First = Scene->CreateEntity("First");
		WEntity* const Second = Scene->CreateEntity("Second");
		WEntity* const Child = Scene->CreateEntity("Child", First);
		AddLabelled<WTestComponent>(*Second, "S");
		AddLabelled<WTestComponent>(*Child, "C");
		AddLabelled<WTestComponent>(*First, "F1");
		AddLabelled<WTestComponent>(*First, "F2");
		// Each component is told with the game over already.
		FTestComponentStats::Get().OnEndPlay = [](WTestComponent& Component, EEndPlayReason)
		{
			if (Component.GetScene()->IsPlaying())
			{
				FTestComponentStats::Get().Events.push_back("Still playing");
			}
		};
		Scene->BeginPlay(QuarterSecond);
		Scene->StepFrame();
		TakeEvents();

		Scene->EndPlay();

		CHECK_FALSE(Scene->IsPlaying());
		CHECK(TakeEvents() ==
			FEvents{"EndPlay S PlayEnded", "EndPlay C PlayEnded", "EndPlay F2 PlayEnded", "EndPlay F1 PlayEnded"});
		for (const WComponent* const Each : Scene->GetComponents<WComponent>())
		{
			CHECK_FALSE(Each->HasBegunPlay());
		}
	}

	TEST_CASE("A component that destroys its own entity as the game ends is told EndPlay once")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestComponent>(*Entity, "Before");
		AddLabelled<WTestComponent>(*Entity, "Leaving");
		FTestComponentStats::Get().OnEndPlay = [](WTestComponent& Component, EEndPlayReason Reason)
		{
			if (Component.Label == "Leaving" && Reason == EEndPlayReason::PlayEnded)
			{
				Component.GetOwner()->Destroy();
			}
		};
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Scene->EndPlay();

		// The component that comes before it on the entity, which has not been told yet, leaves the game with the
		// entity instead.
		CHECK_FALSE(IsValid(Entity));
		CHECK(TakeEvents() ==
			FEvents{"EndPlay Leaving PlayEnded", "EndPlay Before Destroyed", "Detached Before", "Detached Leaving"});
	}

	TEST_CASE("A component that has not begun play is not told that the game ended, and nothing begins as it ends")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestComponent>(*Entity, "Playing");
		FTestComponentStats::Get().OnEndPlay = [](WTestComponent& Component, EEndPlayReason)
		{
			Component.GetOwner()->AddComponent<WTestComponent>()->Label = "Late";
		};
		Scene->BeginPlay(QuarterSecond);
		const WTestComponent* const Pending = AddLabelled<WTestComponent>(*Entity, "Pending");
		TakeEvents();

		Scene->EndPlay();

		CHECK(TakeEvents() == FEvents{"EndPlay Playing PlayEnded", "Attached Unnamed"});
		CHECK_FALSE(Pending->HasBegunPlay());
		REQUIRE(Entity->GetComponents().size() == 3);
		CHECK_FALSE(Entity->GetComponents()[2]->HasBegunPlay());
	}

	TEST_CASE("After EndPlay the scene can play again, from frame 0")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		const WTestPlayComponent* const Ticking =
			AddLabelled<WTestPlayComponent>(*Scene->CreateEntity("Entity"), "Again");
		Scene->BeginPlay(QuarterSecond);
		Scene->StepFrame();
		Scene->StepFrame();
		Scene->EndPlay();
		TakeEvents();

		Scene->BeginPlay(0.5);

		CHECK(Scene->IsPlaying());
		CHECK(Scene->GetPlayedFrameCount() == 0);
		CHECK(Scene->GetPlayedSeconds() == 0.0);
		CHECK(Scene->GetFrameSeconds() == 0.5);
		Scene->StepFrame();
		CHECK(Scene->GetPlayedSeconds() == 0.5);
		CHECK(TakeEvents() == FEvents{"BeginPlay Again", "Tick Again 1 0.5"});
		CHECK(Ticking->TickCount == 3);
		Scene->EndPlay();
	}

	TEST_CASE("Destroying a scene that plays ends its game first")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestComponent>(*Entity, "First");
		AddLabelled<WTestComponent>(*Entity, "Second");
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		Scene->Destroy();

		CHECK_FALSE(Scene->IsPlaying());
		CHECK(TakeEvents() ==
			FEvents{"EndPlay Second PlayEnded", "EndPlay First PlayEnded", "Detached First", "Detached Second"});
	}

	TEST_CASE("EndPlay does nothing to a scene that does not play")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		AddLabelled<WTestComponent>(*Scene->CreateEntity("Entity"), "Idle");
		TakeEvents();

		Scene->EndPlay();
		CHECK(TakeEvents().empty());

		// Nor the second time after a game.
		Scene->BeginPlay(QuarterSecond);
		Scene->EndPlay();
		CHECK(TakeEvents() == FEvents{"BeginPlay Idle", "EndPlay Idle PlayEnded"});
		Scene->EndPlay();
		CHECK(TakeEvents().empty());
		CHECK_FALSE(Scene->IsPlaying());
		CHECK(Capture.GetMessages().empty());
	}

	TEST_CASE("Loading a scene, copying an entity and adding a component begin nothing outside a game")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestPlayComponent>(*Entity, "Original");

		std::string Error;
		WScene* const Loaded = FSceneSerializer::LoadFromText(FSceneSerializer::SaveToText(*Scene.Get()), Error);
		REQUIRE(Loaded != nullptr);
		REQUIRE(FSceneSerializer::DuplicateEntity(*Entity) != nullptr);
		AddLabelled<WTestComponent>(*Entity, "Added");

		CHECK_FALSE(Loaded->IsPlaying());
		for (const WScene* const Each : {Scene.Get(), Loaded})
		{
			for (const WComponent* const Component : Each->GetComponents<WComponent>())
			{
				CHECK_FALSE(Component->HasBegunPlay());
			}
		}
		// Loaded and copied components are attached with their properties set.
		CHECK(
			TakeEvents() == FEvents{"Attached Unnamed", "Attached Original", "Attached Original", "Attached Unnamed"});
		Loaded->Destroy();
	}

	TEST_CASE("A copy made during a game begins play at the next frame, with the properties of the original")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Entity = Scene->CreateEntity("Entity");
		AddLabelled<WTestComponent>(*Entity, "Original");
		Scene->BeginPlay(QuarterSecond);
		TakeEvents();

		const WEntity* const Copy = FSceneSerializer::DuplicateEntity(*Entity);

		REQUIRE(Copy != nullptr);
		const WTestComponent* const Copied = Copy->FindComponent<WTestComponent>();
		REQUIRE(Copied != nullptr);
		CHECK_FALSE(Copied->HasBegunPlay());
		Scene->StepFrame();
		CHECK(Copied->HasBegunPlay());
		CHECK(TakeEvents() == FEvents{"Attached Original", "BeginPlay Original"});
		Scene->EndPlay();
	}

	TEST_CASE("Two scenes loaded from one text and played for a hundred frames save as the same text")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WEntity* const Mover = Scene->CreateEntity("Mover");
		WEntity* const Rider = Scene->CreateEntity("Rider", Mover);
		AddLabelled<WTestPlayComponent>(*Mover, "Mover")->Velocity = FVector(1.1, -2.3, 0.7);
		WTestPlayComponent* const Riding = AddLabelled<WTestPlayComponent>(*Rider, "Rider");
		Riding->Velocity = FVector(-0.3, 0.0, 4.9);
		Riding->TargetEntity = Mover;
		const std::string Text = FSceneSerializer::SaveToText(*Scene.Get());

		std::vector<std::string> PlayedTexts;
		for (int32 Run = 0; Run < 2; ++Run)
		{
			std::string Error;
			WScene* const Loaded = FSceneSerializer::LoadFromText(Text, Error);
			REQUIRE(Loaded != nullptr);
			Loaded->BeginPlay();
			for (int32 Frame = 0; Frame < 100; ++Frame)
			{
				Loaded->StepFrame();
			}
			Loaded->EndPlay();
			PlayedTexts.push_back(FSceneSerializer::SaveToText(*Loaded));
			Loaded->Destroy();
		}

		CHECK(PlayedTexts[0] == PlayedTexts[1]);
		// What they played is in the text.
		CHECK(PlayedTexts[0] != Text);
		CHECK(PlayedTexts[0].find("\"TickCount\": 100") != std::string::npos);
	}

	TEST_CASE("BeginPlay is a fatal error on a scene that plays already or has been destroyed")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;

		Scene->BeginPlay();
		CHECK_THROWS_AS(Scene->BeginPlay(), FFatalErrorException);
		CHECK(LastMessageMentions(Capture, "it plays already"));
		CHECK(Scene->IsPlaying());

		Scene->Destroy();
		CHECK_FALSE(Scene->IsPlaying());
		CHECK_THROWS_AS(Scene->BeginPlay(), FFatalErrorException);
		CHECK(LastMessageMentions(Capture, "it has been destroyed"));
		CHECK_FALSE(Scene->IsPlaying());
	}

	TEST_CASE("BeginPlay is a fatal error with frames that do not last a number of seconds above zero")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;

		for (const double FrameSeconds : {0.0,
				 -0.0,
				 -0.25,
				 std::numeric_limits<double>::quiet_NaN(),
				 std::numeric_limits<double>::infinity(),
				 -std::numeric_limits<double>::infinity()})
		{
			CAPTURE(FrameSeconds);
			CHECK_THROWS_AS(Scene->BeginPlay(FrameSeconds), FFatalErrorException);
			CHECK(LastMessageMentions(Capture, "a frame lasts a number of seconds above zero"));
			CHECK_FALSE(Scene->IsPlaying());
			CHECK(Scene->GetFrameSeconds() == WScene::DefaultFrameSeconds);
		}

		// The shortest length above zero is one.
		Scene->BeginPlay(std::numeric_limits<double>::denorm_min());
		CHECK(Scene->IsPlaying());
		Scene->EndPlay();
	}

	TEST_CASE("StepFrame is a fatal error on a scene that does not play")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;

		CHECK_THROWS_AS(Scene->StepFrame(), FFatalErrorException);
		CHECK(LastMessageMentions(Capture, "it does not play"));

		// After a game as well.
		Scene->BeginPlay();
		Scene->EndPlay();
		CHECK_THROWS_AS(Scene->StepFrame(), FFatalErrorException);
		CHECK(LastMessageMentions(Capture, "it does not play"));
		CHECK(Scene->GetPlayedFrameCount() == 0);
	}

	TEST_CASE("A component cannot begin, step or end the game it plays in, and the game can still end after it tried")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		FTestComponentStats& Stats = FTestComponentStats::Get();

		SUBCASE("From its BeginPlay")
		{
			for (const FPlayCall& PlayCall : PlayCalls)
			{
				CAPTURE(PlayCall.Name);
				const FScopedTestScene Scene;
				AddLabelled<WTestPlayComponent>(*Scene->CreateEntity("Entity"), "Inside");
				Stats.OnBeginPlay = [PlayCall](WTestComponent& Component)
				{
					PlayCall.Call(*Component.GetScene());
				};

				CHECK_THROWS_AS(Scene->BeginPlay(QuarterSecond), FFatalErrorException);

				CHECK(LastMessageMentions(Capture, FromInsideMessage));
				Stats.OnBeginPlay = nullptr;
				TakeEvents();
				CHECK(Scene->IsPlaying());
				CHECK_NOTHROW(Scene->StepFrame());
				CHECK_NOTHROW(Scene->EndPlay());
				CHECK(TakeEvents() == FEvents{"Tick Inside 1 0.25", "EndPlay Inside PlayEnded"});
			}
		}
		SUBCASE("From its Tick")
		{
			for (const FPlayCall& PlayCall : PlayCalls)
			{
				CAPTURE(PlayCall.Name);
				const FScopedTestScene Scene;
				AddLabelled<WTestPlayComponent>(*Scene->CreateEntity("Entity"), "Inside");
				Scene->BeginPlay(QuarterSecond);
				Stats.OnTick = [PlayCall](WTestComponent& Component)
				{
					PlayCall.Call(*Component.GetScene());
				};

				CHECK_THROWS_AS(Scene->StepFrame(), FFatalErrorException);

				CHECK(LastMessageMentions(Capture, FromInsideMessage));
				Stats.OnTick = nullptr;
				TakeEvents();
				CHECK_NOTHROW(Scene->EndPlay());
				CHECK_NOTHROW(Scene->BeginPlay(QuarterSecond));
				CHECK_NOTHROW(Scene->EndPlay());
				CHECK(TakeEvents() ==
					FEvents{"EndPlay Inside PlayEnded", "BeginPlay Inside", "EndPlay Inside PlayEnded"});
			}
		}
		SUBCASE("From its EndPlay")
		{
			for (const FPlayCall& PlayCall : PlayCalls)
			{
				CAPTURE(PlayCall.Name);
				const FScopedTestScene Scene;
				const WTestPlayComponent* const Inside =
					AddLabelled<WTestPlayComponent>(*Scene->CreateEntity("Entity"), "Inside");
				Scene->BeginPlay(QuarterSecond);
				Stats.OnEndPlay = [PlayCall](WTestComponent& Component, EEndPlayReason)
				{
					PlayCall.Call(*Component.GetScene());
				};

				CHECK_THROWS_AS(Scene->EndPlay(), FFatalErrorException);

				CHECK(LastMessageMentions(Capture, FromInsideMessage));
				Stats.OnEndPlay = nullptr;
				TakeEvents();
				// The game ended for the component as it was told.
				CHECK_FALSE(Scene->IsPlaying());
				CHECK_FALSE(Inside->HasBegunPlay());
				CHECK_NOTHROW(Scene->BeginPlay(QuarterSecond));
				CHECK_NOTHROW(Scene->EndPlay());
				CHECK(TakeEvents() == FEvents{"BeginPlay Inside", "EndPlay Inside PlayEnded"});
			}
		}
	}

	TEST_CASE("A component cannot destroy the scene it plays in")
	{
		const FScopedSceneTest SceneTest;
		const FScopedLogCapture Capture;
		const FScopedTestScene Scene;
		AddLabelled<WTestPlayComponent>(*Scene->CreateEntity("Entity"), "Inside");
		Scene->BeginPlay(QuarterSecond);
		FTestComponentStats::Get().OnTick = [](WTestComponent& Component)
		{
			Component.GetScene()->Destroy();
		};

		CHECK_THROWS_AS(Scene->StepFrame(), FFatalErrorException);

		CHECK(LastMessageMentions(Capture, "cannot be destroyed from inside a component's BeginPlay, Tick or EndPlay"));
		FTestComponentStats::Get().OnTick = nullptr;
	}
}
