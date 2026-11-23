#pragma once

#include "CoreMinimal.h"

#include "Object/Class.h"
#include "Object/GarbageCollectorSettings.h"
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

	/** Returns the registered classes, in registration order: a class always comes after its base class. */
	const std::vector<WClass*>& GetClasses() const
	{
		return Classes;
	}

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

	/**
	 * Called whenever a strong reference to Object, which is not null, is about to be stored. Returns the pointer
	 * to store: Object, or null if Object may no longer be referenced.
	 */
	WObject* HandleReferenceStored(WObject* Object);

	// The collector. See FGarbageCollector for the meaning of each function.
	void TickCollection(double DeltaSeconds);
	void RequestCollection();
	FGarbageCollectorSettings GetCollectorSettings() const;
	void SetCollectorSettings(const FGarbageCollectorSettings& Settings);
	void CollectGarbage();
	bool IsCollectionInProgress() const;
	void StartCollection();
	bool AdvanceCollection(int32 MaxWorkUnits);
	int32 GetObjectCount() const;
	int32 ReportLeakedObjects();

private:
	/**
	 * What a collection is doing. Objects are marked first; once marking is complete the unmarked objects are
	 * known to be garbage, and they are notified and then purged.
	 */
	enum class ECollectionPhase : uint8
	{
		/** No collection is in progress. */
		Idle,

		/** Finding every object that is reachable from the root set. */
		Marking,

		/** Calling BeginDestroy on every unreachable object. */
		Notifying,

		/** Destructing and freeing every unreachable object. */
		Purging,
	};

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

	/** Returns true once marking has finished and Object was not reached: it is garbage and will be purged. */
	bool IsUnreachable(const WObject* Object) const
	{
		return (Phase == ECollectionPhase::Notifying || Phase == ECollectionPhase::Purging) && !IsMarked(Object);
	}

	bool IsClassObject(const WObject* Object) const
	{
		return Object->ClassPrivate->IsChildOf(ClassClass);
	}

	/** Records that Object is reachable and queues it to have its own references followed. */
	void MarkObject(WObject* Object);

	/** Marks everything Object refers to. */
	void ScanObject(WObject* Object);

	/** Runs the collection in progress, if any, to its end. */
	void FinishCollection();

	/** Each step function does one unit of work for its phase and moves to the next phase when it runs out. */
	void StepMarking();
	void StepNotifying();
	void StepPurging();

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

	ECollectionPhase Phase = ECollectionPhase::Idle;

	/** The next slot to examine in the notifying and purging phases. */
	std::size_t SweepCursor = 0;

	/** Purging walks the table twice: instances first, then classes, because destroying an instance needs its class. */
	bool bIsPurgingClasses = false;

	/** True while a step of the collection is running; guards against collecting from inside the collector. */
	bool bIsAdvancing = false;

	int32 LiveObjectCount = 0;

	FGarbageCollectorSettings CollectorSettings;
	double SecondsSinceLastCollection = 0.0;
	bool bIsCollectionRequested = false;
};
