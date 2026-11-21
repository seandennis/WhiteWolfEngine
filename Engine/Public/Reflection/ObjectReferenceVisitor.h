#pragma once

#include "CoreMinimal.h"

class FObjectPtr;

/**
 * Receives the strong object references held by an object, one at a time.
 *
 * The garbage collector is one visitor: it marks each referenced object as reachable, and clears references to
 * destroyed objects. A visitor may change the reference it is given.
 */
class ENGINE_API IObjectReferenceVisitor
{
public:
	virtual ~IObjectReferenceVisitor() = default;

	/** Called for each reference, including null ones. */
	virtual void VisitObjectReference(FObjectPtr& Reference) = 0;
};
