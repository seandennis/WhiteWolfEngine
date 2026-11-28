#include "Scene/TestSceneObjects.h"

#include "Object/ObjectGlobals.h"
#include "Scene/Scene.h"

#include <format>

FTestComponentStats& FTestComponentStats::Get()
{
	static FTestComponentStats Stats;
	return Stats;
}

void WTestComponent::OnAttached()
{
	Super::OnAttached();
	FTestComponentStats::Get().Events.push_back("Attached " + Label);
}

void WTestComponent::OnDetached()
{
	FTestComponentStats& Stats = FTestComponentStats::Get();
	Stats.Events.push_back("Detached " + Label);
	if (GetOwner() != nullptr)
	{
		++Stats.DetachedWithOwnerCount;
	}

	Super::OnDetached();
}

void WTestComponent::BeginPlay()
{
	Super::BeginPlay();

	FTestComponentStats& Stats = FTestComponentStats::Get();
	Stats.Events.push_back("BeginPlay " + Label);
	if (Stats.OnBeginPlay)
	{
		Stats.OnBeginPlay(*this);
	}
}

void WTestComponent::Tick(double DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FTestComponentStats& Stats = FTestComponentStats::Get();
	Stats.Events.push_back(std::format("Tick {} {} {}", Label, GetScene()->GetPlayedFrameCount(), DeltaSeconds));
	if (Stats.OnTick)
	{
		Stats.OnTick(*this);
	}
}

void WTestComponent::EndPlay(EEndPlayReason Reason)
{
	FTestComponentStats& Stats = FTestComponentStats::Get();
	Stats.Events.push_back(
		std::format("EndPlay {} {}", Label, Reason == EEndPlayReason::PlayEnded ? "PlayEnded" : "Destroyed"));
	if (Stats.OnEndPlay)
	{
		Stats.OnEndPlay(*this, Reason);
	}

	Super::EndPlay(Reason);
}

WTestPlayComponent::WTestPlayComponent()
{
	bCanEverTick = true;
}

void WTestPlayComponent::Tick(double DeltaSeconds)
{
	++TickCount;
	WEntity* const Entity = GetOwner();
	FTransform Transform = Entity->GetRelativeTransform();
	Transform.Location += Velocity * DeltaSeconds;
	Entity->SetRelativeTransform(Transform);

	// Records the frame and calls the hook, which then sees what this frame did.
	Super::Tick(DeltaSeconds);
}

FScopedSceneTest::FScopedSceneTest()
{
	CollectGarbage();
	FTestComponentStats::Get() = FTestComponentStats();
}

FScopedSceneTest::~FScopedSceneTest()
{
	// The hooks of a test may refer to what only that test has.
	FTestComponentStats::Get() = FTestComponentStats();
	CollectGarbage();
}

FScopedTestScene::FScopedTestScene()
	: Scene(NewObject<WScene>())
{
	Scene->AddToRoot();
}

FScopedTestScene::~FScopedTestScene()
{
	// Destroying the scene removes it from the root set.
	Scene->Destroy();
}
