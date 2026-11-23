#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

class WClass;

/**
 * The classes of components that the editor gives entities: what Component.Add accepts, what Component.ListClasses
 * lists and what the Details panel offers.
 */
class FComponentClasses
{
public:
	/**
	 * Returns true if an entity can be given a component of this class: a class that derives from WComponent, is
	 * not abstract and does not stand in for something else, as WMissingComponent does.
	 *
	 * @param OutReason Receives a sentence that says why not, if not.
	 */
	static bool CanBeAdded(const WClass& Class, std::string& OutReason);

	/** Returns every registered class that CanBeAdded accepts, in the order of their names. */
	static std::vector<WClass*> GetAddable();
};
