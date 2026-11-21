#include "Object/ObjectSystem.h"

#include "Object/ObjectPtr.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <ranges>
#include <utility>

constinit FLogCategory LogObject("LogObject", ELogVerbosity::Log);

namespace
{
	/**
	 * A request from the object system to the WObject constructor that is about to run. A constructor that finds no
	 * request was not started by the object system.
	 */
	struct FPendingConstruction
	{
		FObjectSystem* System = nullptr;
		WClass* Class = nullptr;
		bool bIsPending = false;
	};

	constinit FPendingConstruction PendingConstruction;

	/** Restores the construction request of an enclosing NewObject when a nested one finishes. */
	class FScopedConstructionRequest
	{
	public:
		explicit FScopedConstructionRequest(const FPendingConstruction& Request)
			: Outer(PendingConstruction)
		{
			PendingConstruction = Request;
		}

		~FScopedConstructionRequest()
		{
			PendingConstruction = Outer;
		}

		FScopedConstructionRequest(const FScopedConstructionRequest&) = delete;
		FScopedConstructionRequest& operator=(const FScopedConstructionRequest&) = delete;

	private:
		FPendingConstruction Outer;
	};

	/** Frees an object's memory unless ownership is taken with Release. */
	class FScopedObjectMemory
	{
	public:
		FScopedObjectMemory(std::size_t Size, std::size_t InAlignment)
			: Memory(::operator new(Size, std::align_val_t(InAlignment)))
			, Alignment(InAlignment)
		{
		}

		~FScopedObjectMemory()
		{
			if (Memory != nullptr)
			{
				::operator delete(Memory, std::align_val_t(Alignment));
			}
		}

		FScopedObjectMemory(const FScopedObjectMemory&) = delete;
		FScopedObjectMemory& operator=(const FScopedObjectMemory&) = delete;

		void* Get() const
		{
			return Memory;
		}

		void Release()
		{
			Memory = nullptr;
		}

	private:
		void* Memory;
		std::size_t Alignment;
	};

	/** Sets a flag for the duration of a scope. */
	class FScopedFlag
	{
	public:
		explicit FScopedFlag(bool& bInFlag)
			: bFlag(bInFlag)
		{
			bFlag = true;
		}

		~FScopedFlag()
		{
			bFlag = false;
		}

		FScopedFlag(const FScopedFlag&) = delete;
		FScopedFlag& operator=(const FScopedFlag&) = delete;

	private:
		bool& bFlag;
	};
}

FObjectSystem& FObjectSystem::Get()
{
	static FObjectSystem Instance;
	return Instance;
}

FObjectSystem::FObjectSystem()
{
	// WObject's class is a WClass, and WClass derives from WObject. Neither class object can be described before
	// both exist, so the first two are created without a class and wired together by hand.
	ObjectClass = ConstructClassObject();
	ClassClass = ConstructClassObject();
	ObjectClass->ClassPrivate = ClassClass;
	ClassClass->ClassPrivate = ClassClass;

	FNativeClassDescription ObjectDescription;
	ObjectDescription.Name = "WObject";
	ObjectDescription.Size = sizeof(WObject);
	ObjectDescription.Alignment = alignof(WObject);
	ObjectDescription.Construct = &FObjectSystem::ConstructPlainObject;
	InitializeClass(ObjectClass, ObjectDescription, nullptr);

	// Class objects are created by registration only, so WClass has no construct function for NewObject to use.
	FNativeClassDescription ClassDescription;
	ClassDescription.Name = "WClass";
	ClassDescription.Size = sizeof(WClass);
	ClassDescription.Alignment = alignof(WClass);
	InitializeClass(ClassClass, ClassDescription, ObjectClass);
}

FObjectSystem::~FObjectSystem()
{
	// Last-resort cleanup at process exit. An orderly shutdown has already destroyed everything but the classes.
	// Anything else still here is destroyed without BeginDestroy, because the systems that BeginDestroy would
	// release resources to no longer exist.
	RootObjects.clear();
	const std::vector<WClass*> ClassesToDestroy = std::move(Classes);
	Classes.clear();
	ClassesByName.clear();

	for (std::size_t Index = 0; Index < Slots.size(); ++Index)
	{
		WObject* const Object = Slots[Index].Object;
		const bool bIsInstance = Object != nullptr && !IsClassObject(Object);
		if (bIsInstance)
		{
			DestroyObjectNow(Object);
		}
	}

	// Classes go last, derived classes before their bases, and the class of all classes at the very end because
	// destroying any class object needs it.
	for (WClass* const Class : ClassesToDestroy | std::views::reverse)
	{
		if (Class != ClassClass)
		{
			DestroyObjectNow(Class);
		}
	}
	DestroyObjectNow(ClassClass);
}

void FObjectSystem::HandleObjectConstruction(WObject* Object)
{
	WW_CHECKF(PendingConstruction.bIsPending,
		"A WObject was constructed directly. Objects must be created with NewObject<T>().");

	FObjectSystem& System = *PendingConstruction.System;
	Object->ClassPrivate = PendingConstruction.Class;
	PendingConstruction.bIsPending = false;

	System.AllocateSlot(Object);

	// An object created while a collection is in progress is not garbage in that collection. It needs no scanning
	// either: every reference it comes to hold is reported to the collector as it is stored.
	Object->InternalMarkEpoch = System.Phase != ECollectionPhase::Idle ? System.CurrentEpoch : 0;
}

void FObjectSystem::HandleObjectDestruction(WObject* Object)
{
	if (Object->HasInternalFlag(WObject::EInternalFlags::BeingPurged))
	{
		return;
	}

	if (!Object->HasInternalFlag(WObject::EInternalFlags::ConstructionCompleted))
	{
		// The constructor of a derived class failed after the WObject part had been built. The object never
		// existed as far as anyone else is concerned, so it is simply forgotten.
		FObjectSystem& System = Get();
		System.RemoveFromRoot(Object);
		System.ReleaseSlot(Object);
		return;
	}

	WW_LOG(LogObject,
		Fatal,
		"A {} was deleted directly. Objects are destroyed only by the garbage collector.",
		Object->ClassPrivate->GetName());
}

WClass* FObjectSystem::RegisterNativeClass(const FNativeClassDescription& Description)
{
	WW_CHECK(Description.Name != nullptr);
	WW_CHECKF(Description.GetSuperClass != nullptr, "Class {} has no base class", Description.Name);
	WW_CHECKF(FindClass(Description.Name) == nullptr, "A class named {} is already registered", Description.Name);

	// Registers the base class first if this is the first time it is needed.
	WClass* const SuperClass = Description.GetSuperClass();
	WW_CHECKF(SuperClass != nullptr, "The base class of {} could not be registered", Description.Name);

	WClass* const Class = ConstructClassObject();
	InitializeClass(Class, Description, SuperClass);
	return Class;
}

WClass* FObjectSystem::FindClass(std::string_view Name) const
{
	const auto Iterator = ClassesByName.find(std::string(Name));
	return Iterator != ClassesByName.end() ? Iterator->second : nullptr;
}

WObject* FObjectSystem::CreateObject(WClass* Class)
{
	WW_CHECK(Class != nullptr);
	WW_CHECKF(!Class->IsAbstract(), "NewObject: class {} is abstract and cannot be instantiated", Class->GetName());

	return ConstructObject(Class, Class->Size, Class->Alignment, Class->ConstructFunction);
}

void FObjectSystem::AddToRoot(WObject* Object)
{
	WW_CHECKF(!IsUnreachable(Object),
		"AddToRoot: the {} is being destroyed and cannot be kept alive",
		Object->ClassPrivate->GetName());
	WW_CHECKF(!Object->HasInternalFlag(WObject::EInternalFlags::PendingDestruction),
		"AddToRoot: the {} has been destroyed and cannot be kept alive",
		Object->ClassPrivate->GetName());

	if (Object->HasInternalFlag(WObject::EInternalFlags::Rooted))
	{
		return;
	}

	Object->SetInternalFlag(WObject::EInternalFlags::Rooted);
	RootObjects.push_back(Object);

	// A collection that has already marked its roots must learn about the new one.
	if (Phase == ECollectionPhase::Marking)
	{
		MarkObject(Object);
	}
}

void FObjectSystem::RemoveFromRoot(WObject* Object)
{
	if (!Object->HasInternalFlag(WObject::EInternalFlags::Rooted))
	{
		return;
	}

	Object->ClearInternalFlag(WObject::EInternalFlags::Rooted);
	std::erase(RootObjects, Object);
}

void FObjectSystem::DestroyObject(WObject* Object)
{
	WW_CHECKF(!IsClassObject(Object), "Class {} cannot be destroyed", static_cast<const WClass*>(Object)->GetName());

	Object->SetInternalFlag(WObject::EInternalFlags::PendingDestruction);
	RemoveFromRoot(Object);
}

bool FObjectSystem::IsObjectValid(const WObject* Object) const
{
	return !Object->HasInternalFlag(WObject::EInternalFlags::PendingDestruction) && !IsUnreachable(Object);
}

void FObjectSystem::GetWeakReference(const WObject* Object, int32& OutObjectIndex, uint32& OutSerialNumber) const
{
	OutObjectIndex = Object->InternalIndex;
	OutSerialNumber = Slots[static_cast<std::size_t>(Object->InternalIndex)].SerialNumber;
}

WObject* FObjectSystem::ResolveWeakReference(int32 ObjectIndex, uint32 SerialNumber) const
{
	if (ObjectIndex < 0 || static_cast<std::size_t>(ObjectIndex) >= Slots.size())
	{
		return nullptr;
	}

	const FObjectSlot& Slot = Slots[static_cast<std::size_t>(ObjectIndex)];
	const bool bIsSameObject = Slot.Object != nullptr && Slot.SerialNumber == SerialNumber;
	return bIsSameObject && IsObjectValid(Slot.Object) ? Slot.Object : nullptr;
}

WObject* FObjectSystem::HandleReferenceStored(WObject* Object)
{
	// Nothing may start referring to an object that is on its way out.
	if (!IsObjectValid(Object))
	{
		return nullptr;
	}

	// While marking, an object that has already been scanned may be given a reference to one that has not been
	// reached yet. Marking the target here is what keeps it from being missed.
	if (Phase == ECollectionPhase::Marking)
	{
		MarkObject(Object);
	}

	return Object;
}

void FObjectSystem::TickCollection(double DeltaSeconds)
{
	WW_CHECKF(DeltaSeconds >= 0.0, "Tick was given a negative time step: {}", DeltaSeconds);

	if (Phase == ECollectionPhase::Idle)
	{
		SecondsSinceLastCollection += DeltaSeconds;
		const bool bIsDue = bIsCollectionRequested || SecondsSinceLastCollection >= CollectorSettings.IntervalSeconds;
		if (!bIsDue)
		{
			return;
		}

		StartCollection();
	}

	// Reading the clock costs more than examining an object, so it is read once per batch of work.
	constexpr int32 WorkUnitsPerClockRead = 64;
	const std::chrono::duration<double> TimeBudget(CollectorSettings.TimeBudgetSeconds);
	const std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();

	bool bIsFinished = false;
	bool bHasTimeLeft = true;
	while (!bIsFinished && bHasTimeLeft)
	{
		bIsFinished = AdvanceCollection(WorkUnitsPerClockRead);
		bHasTimeLeft = std::chrono::steady_clock::now() - Start < TimeBudget;
	}
}

void FObjectSystem::RequestCollection()
{
	bIsCollectionRequested = true;
}

FGarbageCollectorSettings FObjectSystem::GetCollectorSettings() const
{
	return CollectorSettings;
}

void FObjectSystem::SetCollectorSettings(const FGarbageCollectorSettings& Settings)
{
	WW_CHECKF(
		Settings.IntervalSeconds >= 0.0, "The collection interval must not be negative: {}", Settings.IntervalSeconds);
	WW_CHECKF(Settings.TimeBudgetSeconds > 0.0,
		"The collection time budget must be greater than zero: {}",
		Settings.TimeBudgetSeconds);

	CollectorSettings = Settings;
}

void FObjectSystem::CollectGarbage()
{
	// A collection in progress has already let through objects that have become garbage since it started, so it
	// is finished first, and a fresh collection then finds everything that is unreachable now.
	FinishCollection();
	StartCollection();
	FinishCollection();
}

bool FObjectSystem::IsCollectionInProgress() const
{
	return Phase != ECollectionPhase::Idle;
}

void FObjectSystem::StartCollection()
{
	WW_CHECKF(Phase == ECollectionPhase::Idle, "StartCollection was called while a collection is in progress");

	++CurrentEpoch;
	bIsCollectionRequested = false;
	Phase = ECollectionPhase::Marking;

	for (WObject* const Root : RootObjects)
	{
		MarkObject(Root);
	}
}

bool FObjectSystem::AdvanceCollection(int32 MaxWorkUnits)
{
	WW_CHECKF(!bIsAdvancing,
		"The garbage collector was called while a collection step is running, for example from BeginDestroy");
	const FScopedFlag AdvancingScope(bIsAdvancing);

	for (int32 WorkUnits = 0; WorkUnits < MaxWorkUnits && Phase != ECollectionPhase::Idle; ++WorkUnits)
	{
		switch (Phase)
		{
			case ECollectionPhase::Marking:
				StepMarking();
				break;
			case ECollectionPhase::Notifying:
				StepNotifying();
				break;
			case ECollectionPhase::Purging:
				StepPurging();
				break;
			case ECollectionPhase::Idle:
			default:
				break;
		}
	}

	return Phase == ECollectionPhase::Idle;
}

int32 FObjectSystem::GetObjectCount() const
{
	return LiveObjectCount;
}

int32 FObjectSystem::ReportLeakedObjects()
{
	CollectGarbage();

	int32 LeakedObjectCount = 0;
	for (const FObjectSlot& Slot : Slots)
	{
		const WObject* const Object = Slot.Object;
		if (Object == nullptr || IsClassObject(Object))
		{
			continue;
		}

		++LeakedObjectCount;
		const bool bIsRooted = Object->HasInternalFlag(WObject::EInternalFlags::Rooted);
		WW_LOG(LogObject,
			Error,
			"Leaked object: a {} still exists at shutdown because {}",
			Object->ClassPrivate->GetName(),
			bIsRooted ? "it was never removed from the root set" : "a leaked object refers to it");
	}

	return LeakedObjectCount;
}

WObject* FObjectSystem::ConstructObject(
	WClass* Class, std::size_t Size, std::size_t Alignment, WObject* (*ConstructFunction)(void*))
{
	FScopedObjectMemory Memory(Size, Alignment);

	WObject* Object = nullptr;
	{
		// Constructors may create further objects, so the request of an enclosing NewObject is kept and restored.
		const FScopedConstructionRequest Request(
			FPendingConstruction{.System = this, .Class = Class, .bIsPending = true});
		Object = ConstructFunction(Memory.Get());
		WW_CHECKF(!PendingConstruction.bIsPending, "The construct function did not construct a WObject");
	}

	Object->SetInternalFlag(WObject::EInternalFlags::ConstructionCompleted);
	Memory.Release();
	return Object;
}

WObject* FObjectSystem::ConstructPlainObject(void* Memory)
{
	return ::new (Memory) WObject();
}

WObject* FObjectSystem::ConstructClass(void* Memory)
{
	return ::new (Memory) WClass();
}

WClass* FObjectSystem::ConstructClassObject()
{
	WObject* const Object =
		ConstructObject(ClassClass, sizeof(WClass), alignof(WClass), &FObjectSystem::ConstructClass);
	return static_cast<WClass*>(Object);
}

void FObjectSystem::InitializeClass(WClass* Class, const FNativeClassDescription& Description, WClass* SuperClass)
{
	Class->Name = Description.Name;
	Class->SuperClass = SuperClass;
	Class->Size = Description.Size;
	Class->Alignment = Description.Alignment;
	Class->ConstructFunction = Description.Construct;

	if (Description.RegisterMembers != nullptr)
	{
		Class->bIsRegistering = true;
		Description.RegisterMembers(*Class);
		Class->bIsRegistering = false;
	}

	if (SuperClass != nullptr)
	{
		Class->ReferenceProperties = SuperClass->ReferenceProperties;
	}
	for (const std::unique_ptr<FProperty>& Property : Class->OwnProperties)
	{
		if (Property->ContainsObjectReferences())
		{
			Class->ReferenceProperties.push_back(Property.get());
		}
	}

	Classes.push_back(Class);
	ClassesByName.emplace(Class->Name, Class);

	// Classes written in C++ exist for as long as their module is loaded.
	AddToRoot(Class);
}

void FObjectSystem::DestroyObjectNow(WObject* Object)
{
	// Read everything that is needed from the object and its class before either is gone.
	const std::size_t Alignment = Object->ClassPrivate->Alignment;
	void* const Memory = dynamic_cast<void*>(Object);

	if (IsClassObject(Object))
	{
		// A class object whose registration failed part-way is not in the registry, and must not remove a
		// registered class that happens to share its name.
		WClass* const Class = static_cast<WClass*>(Object);
		std::erase(Classes, Class);
		const auto Iterator = ClassesByName.find(Class->Name);
		if (Iterator != ClassesByName.end() && Iterator->second == Class)
		{
			ClassesByName.erase(Iterator);
		}
	}

	ReleaseSlot(Object);
	Object->SetInternalFlag(WObject::EInternalFlags::BeingPurged);
	Object->~WObject();
	::operator delete(Memory, std::align_val_t(Alignment));
}

void FObjectSystem::AllocateSlot(WObject* Object)
{
	int32 Index = 0;
	if (!FreeSlotIndices.empty())
	{
		Index = FreeSlotIndices.back();
		FreeSlotIndices.pop_back();
	}
	else
	{
		WW_CHECKF(
			Slots.size() < static_cast<std::size_t>(std::numeric_limits<int32>::max()), "The object table is full");
		Index = static_cast<int32>(Slots.size());
		Slots.emplace_back();
	}

	Slots[static_cast<std::size_t>(Index)].Object = Object;
	Object->InternalIndex = Index;
	++LiveObjectCount;
}

void FObjectSystem::ReleaseSlot(WObject* Object)
{
	const int32 Index = Object->InternalIndex;
	WW_CHECK(Index >= 0 && Slots[static_cast<std::size_t>(Index)].Object == Object);

	FObjectSlot& Slot = Slots[static_cast<std::size_t>(Index)];
	Slot.Object = nullptr;
	++Slot.SerialNumber;
	FreeSlotIndices.push_back(Index);
	Object->InternalIndex = -1;
	--LiveObjectCount;
}

void FObjectSystem::FinishCollection()
{
	constexpr int32 Unlimited = std::numeric_limits<int32>::max();

	bool bIsFinished = false;
	while (!bIsFinished)
	{
		bIsFinished = AdvanceCollection(Unlimited);
	}
}

void FObjectSystem::MarkObject(WObject* Object)
{
	if (IsMarked(Object))
	{
		return;
	}

	Object->InternalMarkEpoch = CurrentEpoch;
	PendingObjects.push_back(Object);
}

void FObjectSystem::ScanObject(WObject* Object)
{
	// An object keeps its class alive.
	MarkObject(Object->ClassPrivate);

	for (const FProperty* const Property : Object->ClassPrivate->GetReferenceProperties())
	{
		Property->VisitObjectReferences(Property->GetValuePtr(Object), *this);
	}
}

void FObjectSystem::StepMarking()
{
	if (PendingObjects.empty())
	{
		// Everything reachable has been marked. From this moment the unmarked objects are garbage: they are no
		// longer valid, weak references to them are null, and nothing can refer to them again.
		Phase = ECollectionPhase::Notifying;
		SweepCursor = 0;
		return;
	}

	WObject* const Object = PendingObjects.back();
	PendingObjects.pop_back();
	ScanObject(Object);
}

void FObjectSystem::StepNotifying()
{
	// BeginDestroy may create objects, which can grow the table, so the table size is read on every step.
	if (SweepCursor >= Slots.size())
	{
		Phase = ECollectionPhase::Purging;
		SweepCursor = 0;
		bIsPurgingClasses = false;
		return;
	}

	WObject* const Object = Slots[SweepCursor].Object;
	const bool bNeedsNotification =
		Object != nullptr && !IsMarked(Object) && !Object->HasInternalFlag(WObject::EInternalFlags::BeginDestroyCalled);
	if (bNeedsNotification)
	{
		// The flag is set first so that an object whose BeginDestroy fails is not notified a second time.
		Object->SetInternalFlag(WObject::EInternalFlags::BeginDestroyCalled);
		Object->BeginDestroy();
		WW_CHECKF(Object->HasInternalFlag(WObject::EInternalFlags::BeginDestroyReachedBase),
			"{}::BeginDestroy did not call Super::BeginDestroy()",
			Object->ClassPrivate->GetName());
	}

	++SweepCursor;
}

void FObjectSystem::StepPurging()
{
	if (SweepCursor >= Slots.size())
	{
		if (bIsPurgingClasses)
		{
			Phase = ECollectionPhase::Idle;
			SecondsSinceLastCollection = 0.0;
		}
		else
		{
			bIsPurgingClasses = true;
			SweepCursor = 0;
		}
		return;
	}

	WObject* const Object = Slots[SweepCursor].Object;
	const bool bShouldPurge = Object != nullptr && !IsMarked(Object) && IsClassObject(Object) == bIsPurgingClasses;
	if (bShouldPurge)
	{
		DestroyObjectNow(Object);
	}

	++SweepCursor;
}

void FObjectSystem::VisitObjectReference(FObjectPtr& Reference)
{
	WObject* const Object = Reference.Get();
	if (Object == nullptr)
	{
		return;
	}

	// A destroyed object is not kept alive by the references to it. They are cleared instead, so that nothing
	// points at the object by the time it is freed.
	if (Object->HasInternalFlag(WObject::EInternalFlags::PendingDestruction))
	{
		Reference.Reset();
		return;
	}

	MarkObject(Object);
}
