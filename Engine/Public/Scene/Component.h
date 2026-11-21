#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

class WEntity;
class WScene;

/**
 * The base class of everything that can be added to an entity to give it data and behaviour: a mesh, a light, a
 * rigid body, a script.
 *
 * A component is created with WEntity::AddComponent and belongs to that entity until it is removed or the entity
 * is destroyed. Its WPROPERTY() members are saved with the scene and shown in the editor without further code.
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

protected:
	/** Called once, after the component has been added to its entity. GetOwner returns that entity. */
	virtual void OnAttached();

	/**
	 * Called once, before the component leaves its entity, whether it is removed alone or the entity is destroyed.
	 * GetOwner still returns the entity, which may itself be marked for destruction. Undo here what OnAttached did.
	 */
	virtual void OnDetached();

	void OnDestroyed() override;

private:
	friend class WEntity;

	/** A back-pointer; it is cleared when the component is detached. */
	WPROPERTY(Transient)
	TObjectPtr<WEntity> Owner;

	int64 Id = 0;
};
