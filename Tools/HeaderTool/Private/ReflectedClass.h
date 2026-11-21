#pragma once

#include "Core/CoreTypes.h"
#include "ReflectedFunction.h"
#include "ReflectedProperty.h"

#include <string>
#include <vector>

/** A class marked with WCLASS(), with its reflected members in declaration order. */
struct FReflectedClass
{
	/** The class name, for example "WDoor". */
	std::string Name;

	/** The name of its first base class, for example "WObject". */
	std::string SuperName;

	std::vector<FReflectedProperty> Properties;
	std::vector<FReflectedFunction> Functions;

	/** The line of the WCLASS(). */
	int32 Line = 0;

	bool operator==(const FReflectedClass& Other) const = default;
};
