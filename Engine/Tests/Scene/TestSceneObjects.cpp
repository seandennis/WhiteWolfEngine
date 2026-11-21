#include "Scene/TestSceneObjects.h"

#include "Object/ObjectGlobals.h"
#include "Scene/Scene.h"

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

FScopedSceneTest::FScopedSceneTest()
{
	CollectGarbage();
	FTestComponentStats::Get() = FTestComponentStats();
}

FScopedSceneTest::~FScopedSceneTest()
{
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
