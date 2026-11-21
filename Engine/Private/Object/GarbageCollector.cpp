#include "Object/GarbageCollector.h"

#include "Object/ObjectSystem.h"

void FGarbageCollector::Tick(double DeltaSeconds)
{
	FObjectSystem::Get().TickCollection(DeltaSeconds);
}

void FGarbageCollector::RequestCollection()
{
	FObjectSystem::Get().RequestCollection();
}

FGarbageCollectorSettings FGarbageCollector::GetSettings()
{
	return FObjectSystem::Get().GetCollectorSettings();
}

void FGarbageCollector::SetSettings(const FGarbageCollectorSettings& Settings)
{
	FObjectSystem::Get().SetCollectorSettings(Settings);
}

void FGarbageCollector::CollectGarbage()
{
	FObjectSystem::Get().CollectGarbage();
}

bool FGarbageCollector::IsCollectionInProgress()
{
	return FObjectSystem::Get().IsCollectionInProgress();
}

void FGarbageCollector::StartCollection()
{
	FObjectSystem::Get().StartCollection();
}

bool FGarbageCollector::AdvanceCollection(int32 MaxWorkUnits)
{
	return FObjectSystem::Get().AdvanceCollection(MaxWorkUnits);
}

int32 FGarbageCollector::GetObjectCount()
{
	return FObjectSystem::Get().GetObjectCount();
}

int32 FGarbageCollector::ReportLeakedObjects()
{
	return FObjectSystem::Get().ReportLeakedObjects();
}
