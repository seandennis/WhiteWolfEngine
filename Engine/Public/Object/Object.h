#pragma once

#include "CoreMinimal.h"

class WClass;

/**
 * The base class of every garbage-collected engine object: components, scripts, assets, classes.
 *
 * Lifetime is automatic. Objects are created with NewObject and destroyed by the garbage collector once nothing
 * reachable refers to them; game code never deletes one. An object is reachable if it is in the root set (see
 * AddToRoot) or is referenced, directly or through other objects, by a reflected TObjectPtr property of a reachable
 * object.
 *
 * The object system is single-threaded: create, use and collect objects on the main thread only.
 */
class ENGINE_API WObject
{
public:
	WObject(const WObject&) = delete;
	WObject& operator=(const WObject&) = delete;

	/** Objects are destroyed only by the garbage collector. Deleting one directly is a fatal error. */
	virtual ~WObject();

	/** Returns the class that describes WObject itself. */
	static WClass* StaticClass();

	/** Returns the class of this object: the most derived class it was created as. */
	WClass* GetClass() const
	{
		return ClassPrivate;
	}

	/** Returns true if this object's class is InClass or derives from it. */
	bool IsA(const WClass* InClass) const;

	/** Returns true if this object's class is ObjectType or derives from it. */
	template <typename ObjectType>
	bool IsA() const
	{
		return IsA(ObjectType::StaticClass());
	}

	/**
	 * Adds this object to the root set, so that it and everything it references stay alive until RemoveFromRoot is
	 * called. Adding an object that is already rooted has no effect.
	 */
	void AddToRoot();

	/** Removes this object from the root set. Removing an object that is not rooted has no effect. */
	void RemoveFromRoot();

	/** Returns true if this object is in the root set. */
	bool IsRooted() const;

protected:
	/** Objects are constructed only by NewObject. Constructing one any other way is a fatal error. */
	WObject();

	/**
	 * Called once, before the object is destroyed, to release resources other than memory: GPU buffers, audio
	 * handles, physics bodies. Every other object is still allocated at this point, although objects destroyed in
	 * the same collection may already have released their own resources. Overrides must call Super::BeginDestroy().
	 *
	 * Destructors run later and must not touch any other object.
	 */
	virtual void BeginDestroy();

private:
	friend class FObjectSystem;

	enum class EInternalFlags : uint8
	{
		None = 0,

		/** The object's constructors have all finished. */
		ConstructionCompleted = 1 << 0,

		/** The object is in the root set. */
		Rooted = 1 << 4,

		/** The collector has called BeginDestroy on the object. */
		BeginDestroyCalled = 1 << 1,

		/** WObject::BeginDestroy ran, which proves every override called its Super. */
		BeginDestroyReachedBase = 1 << 2,

		/** The collector is running the object's destructor. */
		BeingPurged = 1 << 3,
	};

	bool HasInternalFlag(EInternalFlags Flag) const
	{
		return (static_cast<uint8>(InternalFlags) & static_cast<uint8>(Flag)) != 0;
	}

	void SetInternalFlag(EInternalFlags Flag)
	{
		InternalFlags = static_cast<EInternalFlags>(static_cast<uint8>(InternalFlags) | static_cast<uint8>(Flag));
	}

	void ClearInternalFlag(EInternalFlags Flag)
	{
		InternalFlags = static_cast<EInternalFlags>(static_cast<uint8>(InternalFlags) & ~static_cast<uint8>(Flag));
	}

	// The data members carry names that derived classes are unlikely to want for a parameter or a local variable,
	// so that code such as RegisterClassMembers(WClass& Class) never hides a member.

	/** The object's class. Set while the object is being constructed. */
	WClass* ClassPrivate = nullptr;

	/** Index of the object's slot in the object table. */
	int32 InternalIndex = -1;

	/** The collection cycle that last found this object reachable. */
	uint32 InternalMarkEpoch = 0;

	EInternalFlags InternalFlags = EInternalFlags::None;
};
