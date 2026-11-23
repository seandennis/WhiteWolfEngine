#include "Commands/ComponentClasses.h"

#include "Object/Class.h"
#include "Scene/Component.h"
#include "Scene/MissingComponent.h"

#include <algorithm>
#include <format>

bool FComponentClasses::CanBeAdded(const WClass& Class, std::string& OutReason)
{
	if (!Class.IsChildOf(WComponent::StaticClass()))
	{
		OutReason = std::format("The class {} is not a component class.", Class.GetName());
		return false;
	}
	if (Class.IsAbstract())
	{
		OutReason =
			std::format("The class {} is abstract: there can be no component of that class itself.", Class.GetName());
		return false;
	}
	if (&Class == WComponent::StaticClass())
	{
		OutReason = std::format("The class {} is what all components have in common and holds nothing by itself. "
								"Add a component of one of the classes that Component.ListClasses returns.",
			Class.GetName());
		return false;
	}
	if (Class.IsChildOf(WMissingComponent::StaticClass()))
	{
		OutReason = std::format("The class {} stands in for the components of a scene file whose class is not in "
								"this build. Loading a scene makes them; they cannot be added.",
			Class.GetName());
		return false;
	}
	return true;
}

std::vector<WClass*> FComponentClasses::GetAddable()
{
	std::vector<WClass*> Classes;
	for (WClass* const Class : WClass::GetAllClasses())
	{
		std::string Reason;
		if (CanBeAdded(*Class, Reason))
		{
			Classes.push_back(Class);
		}
	}

	std::ranges::sort(
		Classes, [](const WClass* Left, const WClass* Right) { return Left->GetName() < Right->GetName(); });
	return Classes;
}
