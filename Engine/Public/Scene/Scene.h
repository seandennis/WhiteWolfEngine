#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectGlobals.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Component.h"
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
 * Destroy() destroys the scene together with everything in it.
 */
WCLASS()
class ENGINE_API WScene : public WObject
{
	WW_DECLARE_CLASS(WScene, WObject)

public:
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

protected:
	void OnDestroyed() override;

private:
	friend class FSceneSerializer;
	friend class WEntity;

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
};
