#include "Scene/Scene.h"

#include "Object/GarbageCollector.h"

#include <algorithm>
#include <cmath>
#include <utility>

constinit FLogCategory LogScene("LogScene", ELogVerbosity::Log);

namespace
{
	/** Appends Entity and then its descendants, parents before children. */
	void AppendEntityTree(WEntity* Entity, std::vector<WEntity*>& OutEntities)
	{
		// An explicit stack, so that a very deep hierarchy cannot overflow the call stack.
		std::vector<WEntity*> Pending{Entity};
		while (!Pending.empty())
		{
			WEntity* const Current = Pending.back();
			Pending.pop_back();
			OutEntities.push_back(Current);

			// Pushed in reverse, so that the first child is the next entity taken.
			const std::vector<TObjectPtr<WEntity>>& Children = Current->GetChildren();
			for (auto Iterator = Children.rbegin(); Iterator != Children.rend(); ++Iterator)
			{
				if (*Iterator != nullptr)
				{
					Pending.push_back(*Iterator);
				}
			}
		}
	}

	/**
	 * Returns the components of a scene for which IsWanted is true, in the order of the scene. The references are
	 * weak, because what a component does in a game may destroy the components that come after it.
	 */
	template <typename PredicateType>
	std::vector<TWeakObjectPtr<WComponent>> GatherComponents(const WScene& Scene, const PredicateType& IsWanted)
	{
		std::vector<TWeakObjectPtr<WComponent>> Gathered;
		for (const WEntity* const Entity : Scene.GetAllEntities())
		{
			for (const TObjectPtr<WComponent>& Component : Entity->GetComponents())
			{
				if (Component != nullptr && IsWanted(*Component))
				{
					Gathered.emplace_back(Component);
				}
			}
		}
		return Gathered;
	}

	/**
	 * Counts a call of a component's BeginPlay, Tick or EndPlay for as long as it runs, also when it ends in a fatal
	 * error that a test has turned into an exception.
	 */
	class FPlayCallScope
	{
	public:
		explicit FPlayCallScope(int32& InDepth)
			: Depth(InDepth)
		{
			++Depth;
		}

		~FPlayCallScope()
		{
			--Depth;
		}

		FPlayCallScope(const FPlayCallScope&) = delete;
		FPlayCallScope& operator=(const FPlayCallScope&) = delete;

	private:
		int32& Depth;
	};
}

const std::string& WScene::GetName() const
{
	return Name;
}

void WScene::SetName(const std::string& NewName)
{
	Name = NewName;
}

WEntity* WScene::CreateEntity(const std::string& EntityName, WEntity* ParentEntity)
{
	if (!IsValid(this))
	{
		WW_LOG(LogScene, Warning, "CreateEntity: scene '{}' has been destroyed", Name);
		return nullptr;
	}
	if (ParentEntity != nullptr)
	{
		if (!IsValid(ParentEntity))
		{
			WW_LOG(LogScene, Warning, "CreateEntity: the parent given for entity '{}' has been destroyed", EntityName);
			return nullptr;
		}
		if (ParentEntity->GetScene() != this)
		{
			WW_LOG(LogScene,
				Warning,
				"CreateEntity: the parent given for entity '{}' is not in scene '{}'",
				EntityName,
				Name);
			return nullptr;
		}
	}

	return CreateEntityWithId(EntityName, ParentEntity, AllocateId());
}

WEntity* WScene::CreateEntityWithId(const std::string& EntityName, WEntity* ParentEntity, int64 EntityId)
{
	WEntity* const Entity = NewObject<WEntity>();
	Entity->Name = EntityName;
	Entity->Id = EntityId;
	Entity->Scene = this;
	Entity->Parent = ParentEntity;
	RegisterObject(EntityId, *Entity);

	if (ParentEntity != nullptr)
	{
		ParentEntity->Children.push_back(Entity);
	}
	else
	{
		RootEntities.push_back(Entity);
	}

	return Entity;
}

const std::vector<TObjectPtr<WEntity>>& WScene::GetRootEntities() const
{
	return RootEntities;
}

std::vector<WEntity*> WScene::GetAllEntities() const
{
	std::vector<WEntity*> Entities;
	for (const TObjectPtr<WEntity>& RootEntity : RootEntities)
	{
		if (RootEntity != nullptr)
		{
			AppendEntityTree(RootEntity, Entities);
		}
	}
	return Entities;
}

WEntity* WScene::FindEntityById(int64 EntityId) const
{
	return Cast<WEntity>(FindObjectById(EntityId));
}

WComponent* WScene::FindComponentById(int64 ComponentId) const
{
	return Cast<WComponent>(FindObjectById(ComponentId));
}

WEntity* WScene::FindEntityByName(std::string_view EntityName) const
{
	const std::vector<WEntity*> Entities = GetAllEntities();
	const auto Found =
		std::ranges::find_if(Entities, [EntityName](const WEntity* Entity) { return Entity->GetName() == EntityName; });
	return Found != Entities.end() ? *Found : nullptr;
}

void WScene::DestroyAllEntities()
{
	// Each entity removes itself from RootEntities as it is destroyed, so the list is taken first.
	const std::vector<TObjectPtr<WEntity>> RemovedEntities = std::exchange(RootEntities, {});
	for (const TObjectPtr<WEntity>& Entity : RemovedEntities)
	{
		if (Entity != nullptr)
		{
			Entity->Destroy();
		}
	}
}

void WScene::BeginPlay(double FrameSeconds)
{
	WW_CHECKF(
		PlayCallDepth == 0, "Scene '{}' cannot begin play from inside a component's BeginPlay, Tick or EndPlay", Name);
	WW_CHECKF(IsValid(this), "Scene '{}' cannot begin play: it has been destroyed", Name);
	WW_CHECKF(!bIsPlaying, "Scene '{}' cannot begin play: it plays already", Name);
	WW_CHECKF(std::isfinite(FrameSeconds) && FrameSeconds > 0.0,
		"Scene '{}' cannot begin play in frames of {} seconds: a frame lasts a number of seconds above zero",
		Name,
		FrameSeconds);

	bIsPlaying = true;
	PlayFrameSeconds = FrameSeconds;
	PlayedFrameCount = 0;

	const std::vector<TWeakObjectPtr<WComponent>> Components =
		GatherComponents(*this, [](const WComponent& Component) { return !Component.bHasBegunPlay; });
	for (const TWeakObjectPtr<WComponent>& Component : Components)
	{
		WComponent* const Existing = Component.Get();
		if (Existing != nullptr)
		{
			BeginComponentPlay(*Existing);
		}
	}
}

void WScene::StepFrame()
{
	WW_CHECKF(PlayCallDepth == 0,
		"Scene '{}' cannot play a frame from inside a component's BeginPlay, Tick or EndPlay",
		Name);
	WW_CHECKF(bIsPlaying, "Scene '{}' cannot play a frame: it does not play", Name);

	// The count goes up first, so that what ticks sees the time that its frame brings the game to.
	++PlayedFrameCount;

	const std::vector<TWeakObjectPtr<WComponent>> Components = GatherComponents(
		*this, [](const WComponent& Component) { return !Component.bHasBegunPlay || Component.bCanEverTick; });

	// What was attached since the last frame begins before anything ticks, so that a component that ticks finds the
	// others of the frame begun.
	for (const TWeakObjectPtr<WComponent>& Component : Components)
	{
		WComponent* const Existing = Component.Get();
		if (Existing != nullptr && !Existing->bHasBegunPlay)
		{
			BeginComponentPlay(*Existing);
		}
	}

	for (const TWeakObjectPtr<WComponent>& Component : Components)
	{
		WComponent* const Existing = Component.Get();
		if (Existing != nullptr && Existing->bCanEverTick)
		{
			TickComponent(*Existing);
		}
	}
}

void WScene::EndPlay()
{
	WW_CHECKF(
		PlayCallDepth == 0, "Scene '{}' cannot end play from inside a component's BeginPlay, Tick or EndPlay", Name);
	if (!bIsPlaying)
	{
		return;
	}

	// The game is over before the first component is told, so that each of them finds the scene no longer playing.
	bIsPlaying = false;

	const std::vector<TWeakObjectPtr<WComponent>> Components =
		GatherComponents(*this, [](const WComponent& Component) { return Component.bHasBegunPlay; });
	for (auto Iterator = Components.rbegin(); Iterator != Components.rend(); ++Iterator)
	{
		WComponent* const Existing = Iterator->Get();
		if (Existing != nullptr)
		{
			EndComponentPlay(*Existing, EEndPlayReason::PlayEnded);
		}
	}
}

bool WScene::IsPlaying() const
{
	return bIsPlaying;
}

double WScene::GetFrameSeconds() const
{
	return PlayFrameSeconds;
}

int64 WScene::GetPlayedFrameCount() const
{
	return PlayedFrameCount;
}

double WScene::GetPlayedSeconds() const
{
	// One multiplication, never a sum of frames, so that the time is exact however the frames were stepped.
	return static_cast<double>(PlayedFrameCount) * PlayFrameSeconds;
}

void WScene::OnDestroyed()
{
	Super::OnDestroyed();

	WW_CHECKF(PlayCallDepth == 0,
		"Scene '{}' cannot be destroyed from inside a component's BeginPlay, Tick or EndPlay",
		Name);
	// A game that is going on ends before the entities go, so that its components are told that the game ended,
	// in the order in which a game ends, rather than one by one that they were destroyed.
	EndPlay();
	DestroyAllEntities();

	// A whole scene has just become garbage. Collecting it soon, rather than when the timer next comes round,
	// frees its memory and resources before the next scene needs them.
	FGarbageCollector::RequestCollection();
}

void WScene::BeginComponentPlay(WComponent& Component)
{
	const FPlayCallScope CallScope(PlayCallDepth);
	// Set before the call, so that a component that destroys itself in BeginPlay is told EndPlay as well.
	Component.bHasBegunPlay = true;
	Component.BeginPlay();
}

void WScene::TickComponent(WComponent& Component)
{
	const FPlayCallScope CallScope(PlayCallDepth);
	Component.Tick(PlayFrameSeconds);
}

void WScene::EndComponentPlay(WComponent& Component, EEndPlayReason Reason)
{
	if (!Component.bHasBegunPlay)
	{
		return;
	}

	const FPlayCallScope CallScope(PlayCallDepth);
	// Cleared before the call, so that the component is told only once, whatever it destroys in EndPlay.
	Component.bHasBegunPlay = false;
	Component.EndPlay(Reason);
}

int64 WScene::AllocateId()
{
	return NextId++;
}

void WScene::RegisterObject(int64 ObjectId, const WObject& Object)
{
	ObjectsById[ObjectId] = FWeakObjectPtr(&Object);
	NextId = std::max(NextId, ObjectId + 1);
}

void WScene::UnregisterObject(int64 ObjectId)
{
	ObjectsById.erase(ObjectId);
}

WObject* WScene::FindObjectById(int64 ObjectId) const
{
	const auto Found = ObjectsById.find(ObjectId);
	return Found != ObjectsById.end() ? Found->second.Get() : nullptr;
}
