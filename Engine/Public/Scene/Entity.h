#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectGlobals.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Scene/Component.h"
#include "Scene/TransformRule.h"

#include <string>
#include <vector>

class WScene;

/**
 * A thing in a scene: a name, a transform, a place in the scene's hierarchy and a list of components.
 *
 * Entities are created with WScene::CreateEntity. An entity's transform is relative to its parent; an entity
 * without a parent is placed directly in the world. Locations are double precision, so an entity can be anywhere in
 * a very large world.
 *
 * Destroy() destroys the entity together with its components and all its descendants, and removes it from its
 * parent at once. Because that changes the lists returned by GetChildren, GetComponents and
 * WScene::GetRootEntities, copy such a list before destroying entities while going through it.
 */
WCLASS()
class ENGINE_API WEntity : public WObject
{
	WW_DECLARE_CLASS(WEntity, WObject)

public:
	WFUNCTION(BlueprintPure, Category = "Entity")
	const std::string& GetName() const;

	WFUNCTION(BlueprintCallable, Category = "Entity")
	void SetName(const std::string& NewName);

	/** Returns the number that identifies this entity within its scene; scene files refer to it by this number. */
	WFUNCTION(BlueprintPure, Category = "Entity")
	int64 GetId() const;

	/** Returns the scene this entity is in, or null once it has been destroyed. */
	WFUNCTION(BlueprintPure, Category = "Entity")
	WScene* GetScene() const;

	/** Returns the parent entity, or null for an entity at the root of its scene. */
	WFUNCTION(BlueprintPure, Category = "Hierarchy")
	WEntity* GetParent() const;

	/** Returns the entities whose parent this is, in the order they were attached. */
	const std::vector<TObjectPtr<WEntity>>& GetChildren() const;

	/**
	 * Gives the entity another parent, or none. The entity becomes the last child of its new parent.
	 *
	 * @param NewParent The new parent, or null to move the entity to the root of the scene.
	 * @param TransformRule Whether the entity keeps its place in the world or its relative transform.
	 * @return False, with a warning in the log and nothing changed, if NewParent is this entity or one of its
	 * descendants, is in another scene, or has been destroyed.
	 */
	bool SetParent(WEntity* NewParent, ETransformRule TransformRule = ETransformRule::KeepWorld);

	/** Returns true if Other is this entity's parent, or its parent's parent, and so on. */
	bool IsDescendantOf(const WEntity* Other) const;

	/** Returns the transform relative to the parent entity, or to the world for an entity without a parent. */
	WFUNCTION(BlueprintPure, Category = "Transform")
	const FTransform& GetRelativeTransform() const;

	WFUNCTION(BlueprintCallable, Category = "Transform")
	void SetRelativeTransform(const FTransform& NewTransform);

	/** Returns the transform in world space: the relative transforms of this entity and all its ancestors combined. */
	WFUNCTION(BlueprintPure, Category = "Transform")
	FTransform GetWorldTransform() const;

	/** Moves the entity so that its world transform becomes NewTransform. Its children move with it. */
	WFUNCTION(BlueprintCallable, Category = "Transform")
	void SetWorldTransform(const FTransform& NewTransform);

	/** Returns the entity's position in world space. */
	WFUNCTION(BlueprintPure, Category = "Transform")
	FVector GetWorldLocation() const;

	/** Moves the entity to a position in world space, keeping its rotation and scale. */
	WFUNCTION(BlueprintCallable, Category = "Transform")
	void SetWorldLocation(const FVector& NewLocation);

	/**
	 * Creates a component of the given class and adds it to this entity.
	 *
	 * @return The new component, or null, with a warning in the log, if ComponentClass is null, abstract or not a
	 * component class, or if this entity has been destroyed.
	 */
	WComponent* AddComponent(WClass* ComponentClass);

	/** Creates a component of type ComponentType and adds it to this entity. */
	template <typename ComponentType>
	ComponentType* AddComponent()
	{
		return static_cast<ComponentType*>(AddComponent(ComponentType::StaticClass()));
	}

	/** Returns the components of this entity, in the order they were added. */
	const std::vector<TObjectPtr<WComponent>>& GetComponents() const;

	/** Returns the first component that is of the given class or of a class derived from it, or null. */
	WComponent* FindComponent(const WClass* ComponentClass) const;

	/** Returns the first component that is a ComponentType, or null. */
	template <typename ComponentType>
	ComponentType* FindComponent() const
	{
		return static_cast<ComponentType*>(FindComponent(ComponentType::StaticClass()));
	}

	/**
	 * Removes a component from this entity and destroys it; the same as calling Destroy on the component.
	 *
	 * @return False if Component is null or does not belong to this entity.
	 */
	bool RemoveComponent(WComponent* Component);

protected:
	void OnDestroyed() override;

private:
	friend class FSceneSerializer;
	friend class WComponent;
	friend class WScene;

	/**
	 * Creates a component with a given identifier and adds it to this entity without telling it; the caller calls
	 * NotifyComponentAttached once the component is ready. ComponentClass must be a concrete component class.
	 */
	WComponent* CreateComponentWithId(WClass* ComponentClass, int64 ComponentId);

	/** Calls OnAttached on a component of this entity. */
	void NotifyComponentAttached(WComponent& Component);

	/** Tells a component that it is leaving this entity and forgets its identifier. Does not touch the list. */
	void DetachComponent(WComponent& Component);

	/** Called by a component that was destroyed on its own. */
	void HandleComponentDestroyed(WComponent& Component);

	/** Returns the list this entity is in: its parent's children, or the scene's root entities. Null if neither. */
	std::vector<TObjectPtr<WEntity>>* GetSiblings() const;

	WPROPERTY()
	std::string Name;

	WPROPERTY()
	FTransform RelativeTransform;

	// The hierarchy is saved by the scene itself, as structure, rather than as property values.

	WPROPERTY(Transient)
	TObjectPtr<WScene> Scene;

	WPROPERTY(Transient)
	TObjectPtr<WEntity> Parent;

	WPROPERTY(Transient)
	std::vector<TObjectPtr<WEntity>> Children;

	WPROPERTY(Transient)
	std::vector<TObjectPtr<WComponent>> Components;

	int64 Id = 0;
};
