#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectGlobals.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Component.h"
#include "Scene/EndPlayReason.h"
#include "Scene/Entity.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/** Log category of scenes, entities and components. */
extern ENGINE_API FLogCategory LogScene;

/**
 * A world: a tree of entities.
 *
 * The scene keeps its entities alive, and they keep their components alive, for as long as the scene itself is
 * reachable; whoever owns a scene holds it in a reflected TObjectPtr or adds it to the root set. Every entity and
 * component of a scene has an identifier, a positive number that is unique within the scene, never reused, and
 * saved with the scene, so that files and tools can refer to an object by it.
 *
 * Destroy() destroys the scene together with everything in it, and asks the garbage collector to start a
 * collection on its next tick instead of waiting for its timer, so that the memory of the old scene is on its way
 * out before a new one is loaded. A scene that plays ends its game first; destroying it from inside a component's
 * BeginPlay, Tick or EndPlay is a fatal error.
 *
 * A scene is also what plays a game. Between BeginPlay and EndPlay it plays in frames (StepFrame), each of which
 * stands for the same length of game time, and in which its components begin play, tick and end play
 * (WComponent::BeginPlay, Tick and EndPlay), in the order of the scene: each root entity followed by its
 * descendants, parents before children, and the components of an entity in the order they were added. Nothing in
 * a frame reads a clock, so the same scene played for the same number of frames comes out the same.
 */
WCLASS()
class ENGINE_API WScene : public WObject
{
	WW_DECLARE_CLASS(WScene, WObject)

public:
	/** The length of a frame of the game unless BeginPlay is given another: a sixtieth of a second. */
	static constexpr double DefaultFrameSeconds = 1.0 / 60.0;

	WFUNCTION(BlueprintPure, Category = "Scene")
	const std::string& GetName() const;

	WFUNCTION(BlueprintCallable, Category = "Scene")
	void SetName(const std::string& NewName);

	/**
	 * Creates an entity in this scene.
	 *
	 * @param EntityName The entity's name. Names need not be unique.
	 * @param ParentEntity The entity to attach it to, or null to put it at the root of the scene.
	 * @return The new entity, or null, with a warning in the log, if this scene or ParentEntity has been destroyed
	 * or ParentEntity is in another scene.
	 */
	WEntity* CreateEntity(const std::string& EntityName, WEntity* ParentEntity = nullptr);

	/** Returns the entities without a parent, in the order they were created or moved to the root. */
	const std::vector<TObjectPtr<WEntity>>& GetRootEntities() const;

	/** Returns every entity of the scene: each root entity followed by its descendants, parents before children. */
	std::vector<WEntity*> GetAllEntities() const;

	/** Returns the entity with the given identifier, or null. */
	WFUNCTION(BlueprintPure, Category = "Scene")
	WEntity* FindEntityById(int64 EntityId) const;

	/** Returns the component with the given identifier, or null. */
	WFUNCTION(BlueprintPure, Category = "Scene")
	WComponent* FindComponentById(int64 ComponentId) const;

	/** Returns the first entity with the given name, in the order of GetAllEntities, or null. */
	WEntity* FindEntityByName(std::string_view EntityName) const;

	/** Returns every component in the scene that is a ComponentType, in the order of GetAllEntities. */
	template <typename ComponentType>
	std::vector<ComponentType*> GetComponents() const
	{
		std::vector<ComponentType*> Result;
		for (const WEntity* const Entity : GetAllEntities())
		{
			for (const TObjectPtr<WComponent>& Component : Entity->GetComponents())
			{
				if (ComponentType* const Typed = Cast<ComponentType>(Component.Get()))
				{
					Result.push_back(Typed);
				}
			}
		}
		return Result;
	}

	/** Destroys every entity in the scene. The scene itself stays usable, and identifiers are not reused. */
	void DestroyAllEntities();

	/**
	 * Begins the game: the scene plays from frame 0, and every component begins play before this returns. Nothing
	 * ticks until the first StepFrame.
	 *
	 * A fatal error if the scene plays already or has been destroyed, if FrameSeconds is not a number of seconds
	 * above zero, or if it is called from inside a component's BeginPlay, Tick or EndPlay.
	 *
	 * @param FrameSeconds The game time that a frame stands for.
	 */
	void BeginPlay(double FrameSeconds = DefaultFrameSeconds);

	/**
	 * Plays one frame. First the components that were attached since the last frame begin play, now that whoever
	 * attached them has set them up; then every component whose class ticks (WComponent::bCanEverTick) ticks. A
	 * component attached during the frame begins at the start of the next one. A component destroyed during the frame
	 * ends play at once and does not tick.
	 *
	 * A fatal error if the scene does not play, or if it is called from inside a component's BeginPlay, Tick or
	 * EndPlay.
	 */
	void StepFrame();

	/**
	 * Ends the game: every component that began play ends it (EEndPlayReason::PlayEnded), in the reverse of the order
	 * of the scene, so that what began first ends last. A component that was attached since the last frame has not
	 * begun, and is not told. The frame count is kept until the scene begins play again, which it can. Destroying a
	 * scene that plays ends its game first. Does nothing if the scene does not play.
	 *
	 * A fatal error if it is called from inside a component's BeginPlay, Tick or EndPlay.
	 */
	void EndPlay();

	/** Returns true from BeginPlay until EndPlay. */
	bool IsPlaying() const;

	/** Returns the game time that a frame stands for: what BeginPlay was given last, or DefaultFrameSeconds. */
	double GetFrameSeconds() const;

	/** Returns the number of frames played since the game began; during a frame, the number of that frame, from 1. */
	int64 GetPlayedFrameCount() const;

	/**
	 * Returns the game time played: the number of frames times the length of a frame, which is exact however the
	 * frames were stepped.
	 */
	double GetPlayedSeconds() const;

protected:
	void OnDestroyed() override;

private:
	friend class FSceneSerializer;
	friend class WEntity;

	/** Has a component begin play, counting the call in PlayCallDepth. */
	void BeginComponentPlay(WComponent& Component);

	/** Has a component tick through one frame, counting the call in PlayCallDepth. */
	void TickComponent(WComponent& Component);

	/** Has a component that began play end it, counting the call in PlayCallDepth. Does nothing for any other. */
	void EndComponentPlay(WComponent& Component, EEndPlayReason Reason);

	/** Creates an entity with a given identifier. ParentEntity, if not null, must be a valid entity of this scene. */
	WEntity* CreateEntityWithId(const std::string& EntityName, WEntity* ParentEntity, int64 EntityId);

	/** Returns an identifier that no object of this scene has had. */
	int64 AllocateId();

	/** Records which object an identifier belongs to, and makes sure the identifier is never allocated again. */
	void RegisterObject(int64 ObjectId, const WObject& Object);

	void UnregisterObject(int64 ObjectId);

	/** Returns the object with the given identifier, or null. */
	WObject* FindObjectById(int64 ObjectId) const;

	WPROPERTY()
	std::string Name;

	WPROPERTY(Transient)
	std::vector<TObjectPtr<WEntity>> RootEntities;

	/** The next identifier to hand out. */
	int64 NextId = 1;

	/** Every entity and component of the scene by identifier. Weak, because the tree above is what owns them. */
	std::unordered_map<int64, FWeakObjectPtr> ObjectsById;

	/** True from BeginPlay until EndPlay. */
	bool bIsPlaying = false;

	/** The game time that a frame stands for. */
	double PlayFrameSeconds = DefaultFrameSeconds;

	/** The number of frames played since the game began. */
	int64 PlayedFrameCount = 0;

	/** How many calls of components' BeginPlay, Tick and EndPlay are running, one inside another. */
	int32 PlayCallDepth = 0;
};
