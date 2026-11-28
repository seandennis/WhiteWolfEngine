#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Scene/EndPlayReason.h"

class WEntity;
class WScene;

/**
 * The base class of everything that can be added to an entity to give it data and behaviour: a mesh, a light, a
 * rigid body, a script.
 *
 * A component is created with WEntity::AddComponent and belongs to that entity until it is removed or the entity
 * is destroyed. Its WPROPERTY() members are saved with the scene and shown in the editor without further code.
 *
 * While its scene plays (WScene::BeginPlay), a component takes part in the game: BeginPlay once, Tick in every frame
 * if its class sets bCanEverTick, and EndPlay once. A scene that is only edited never plays, so OnAttached and
 * OnDetached are for what a component is in a scene, shown and edited, and BeginPlay, Tick and EndPlay for what it
 * does in a game. The scene being edited and a copy of it that plays can exist at the same time.
 */
WCLASS()
class ENGINE_API WComponent : public WObject
{
	WW_DECLARE_CLASS(WComponent, WObject)

public:
	/** Returns the entity this component belongs to, or null once it has been removed from it. */
	WFUNCTION(BlueprintPure, Category = "Component")
	WEntity* GetOwner() const;

	/** Returns the scene of the owning entity, or null. */
	WFUNCTION(BlueprintPure, Category = "Component")
	WScene* GetScene() const;

	/** Returns the number that identifies this component within its scene; scene files refer to it by this number. */
	WFUNCTION(BlueprintPure, Category = "Component")
	int64 GetId() const;

	/**
	 * Returns true while the component takes part in its scene's game: from just before BeginPlay is called until
	 * just before EndPlay is called.
	 */
	bool HasBegunPlay() const;

protected:
	/** Called once, after the component has been added to its entity. GetOwner returns that entity. */
	virtual void OnAttached();

	/**
	 * Called once, before the component leaves its entity, whether it is removed alone or the entity is destroyed.
	 * GetOwner still returns the entity, which may itself be marked for destruction. Undo here what OnAttached did.
	 */
	virtual void OnDetached();

	/**
	 * Called once when the component begins to take part in its scene's game: when the scene begins play, or, for a
	 * component that was attached while the scene plays, at the start of the scene's next frame, before anything
	 * ticks in it. Whoever attached the component has set its properties by then. Overrides call Super::BeginPlay.
	 */
	virtual void BeginPlay();

	/**
	 * Called in every frame of the game between BeginPlay and EndPlay, if the class sets bCanEverTick. Overrides
	 * call Super::Tick.
	 *
	 * @param DeltaSeconds The game time that a frame stands for, the same in every frame (WScene::GetFrameSeconds).
	 */
	virtual void Tick(double DeltaSeconds);

	/**
	 * Called once when a component that began play stops taking part in the game: when the game ends, or when the
	 * component leaves a game that goes on. GetOwner and GetScene still return its entity and scene; OnDetached comes
	 * after, if the component is leaving its entity. Overrides call Super::EndPlay.
	 *
	 * @param Reason Whether the game ended or the component left it.
	 */
	virtual void EndPlay(EEndPlayReason Reason);

	void OnDestroyed() override;

	/**
	 * True for a class whose components act in every frame of a game; set it in the constructor of such a class.
	 * Only those are ticked, so that a scene of meshes and lights costs next to nothing to play.
	 */
	bool bCanEverTick = false;

private:
	friend class WEntity;
	friend class WScene;

	/** A back-pointer; it is cleared when the component is detached. */
	WPROPERTY(Transient)
	TObjectPtr<WEntity> Owner;

	int64 Id = 0;

	/**
	 * True from just before BeginPlay until just before EndPlay. Not a property, so that neither a scene file nor a
	 * copy of a component has begun play.
	 */
	bool bHasBegunPlay = false;
};
