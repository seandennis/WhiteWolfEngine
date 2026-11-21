#pragma once

#include "CoreMinimal.h"

#include "Object/Class.h"
#include "Object/Object.h"
#include "Reflection/ObjectReferenceVisitor.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/** Log category of the object system. */
extern FLogCategory LogObject;

/**
 * The object table, the class registry and the garbage collector. Internal to the Engine module; the rest of the
 * engine reaches it through WObject, WClass and the functions in ObjectGlobals.h.
 *
 * There is one instance, created on first use. It is used from the main thread only.
 */
class FObjectSystem final : private IObjectReferenceVisitor
{
public:
	/** Returns the object system, creating it on first use. */
	static FObjectSystem& Get();

	FObjectSystem();
	~FObjectSystem() override;

	FObjectSystem(const FObjectSystem&) = delete;
	FObjectSystem& operator=(const FObjectSystem&) = delete;

	/** Called by WObject's constructor: gives the object its class and its slot in the object table. */
	static void HandleObjectConstruction(WObject* Object);

	/** Called by WObject's destructor: verifies that the collector is the one destroying the object. */
	static void HandleObjectDestruction(WObject* Object);

	/** Returns the class object of WObject. */
	WClass* GetObjectClass() const
	{
		return ObjectClass;
	}

	/** Returns the class object of WClass. */
	WClass* GetClassClass() const
	{
		return ClassClass;
	}

	WClass* RegisterNativeClass(const FNativeClassDescription& Description);
	WClass* FindClass(std::string_view Name) const;

	/** Creates an instance of Class, which must not be null or abstract. */
	WObject* CreateObject(WClass* Class);

	void AddToRoot(WObject* Object);
	void RemoveFromRoot(WObject* Object);

	/** Marks Object for destruction; see WObject::Destroy. */
	void DestroyObject(WObject* Object);

	/** Returns false if Object is marked for destruction or is being destroyed by the running collection. */
	bool IsObjectValid(const WObject* Object) const;

	/** Returns the slot and serial number that identify Object for a weak reference. */
	void GetWeakReference(const WObject* Object, int32& OutObjectIndex, uint32& OutSerialNumber) const;

	/** Returns the object a weak reference identifies, or null if it is gone or no longer valid. */
	WObject* ResolveWeakReference(int32 ObjectIndex, uint32 SerialNumber) const;

	/** Runs one complete collection: marks from the roots, then destroys every object that was not reached. */
	void CollectGarbage();

private:
	/** One entry of the object table. */
	struct FObjectSlot
	{
		/** The object in this slot, or null if the slot is free. */
		WObject* Object = nullptr;

		/** Changes every time the slot is released, so a weak reference can tell that its object is gone. */
		uint32 SerialNumber = 1;
	};

	/** Allocates memory for an object, runs ConstructFunction in it and returns the finished object. */
	WObject* ConstructObject(
		WClass* Class, std::size_t Size, std::size_t Alignment, WObject* (*ConstructFunction)(void*));

	/** Construct functions for the two classes that the object system builds itself. */
	static WObject* ConstructPlainObject(void* Memory);
	static WObject* ConstructClass(void* Memory);

	/** Constructs a class object. Its fields are filled in afterwards by InitializeClass. */
	WClass* ConstructClassObject();

	/** Fills in a class object from its description, registers its members and adds it to the registry. */
	void InitializeClass(WClass* Class, const FNativeClassDescription& Description, WClass* SuperClass);

	/** Runs the object's destructor, frees its memory and releases its slot. */
	void DestroyObjectNow(WObject* Object);

	void AllocateSlot(WObject* Object);
	void ReleaseSlot(WObject* Object);

	bool IsMarked(const WObject* Object) const
	{
		return Object->InternalMarkEpoch == CurrentEpoch;
	}

	/** Records that Object is reachable and queues it to have its own references followed. */
	void MarkObject(WObject* Object);

	/** Marks everything Object refers to. */
	void ScanObject(WObject* Object);

	void MarkReachableObjects();
	void NotifyUnreachableObjects();
	void PurgeUnreachableObjects();

	// IObjectReferenceVisitor
	void VisitObjectReference(FObjectPtr& Reference) override;

	std::vector<FObjectSlot> Slots;
	std::vector<int32> FreeSlotIndices;

	/** Registered classes, in registration order: a class always comes after its base class. */
	std::vector<WClass*> Classes;
	std::unordered_map<std::string, WClass*> ClassesByName;
	WClass* ObjectClass = nullptr;
	WClass* ClassClass = nullptr;

	std::vector<WObject*> RootObjects;

	/** Identifies the current collection. An object is marked when its MarkEpoch equals this value. */
	uint32 CurrentEpoch = 0;

	/** Objects that are marked but whose references have not been followed yet. */
	std::vector<WObject*> PendingObjects;

	/** True for the whole of a collection. */
	bool bIsCollecting = false;

	/** True once marking has finished, while the unmarked objects are being notified and destroyed. */
	bool bIsSweeping = false;
};
