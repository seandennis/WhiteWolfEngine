#pragma once

#include "ReflectedClass.h"

#include <string>
#include <vector>

/** A header file and the reflected classes it declares. */
struct FReflectedHeader
{
	/** The path to write in an #include directive, with forward slashes: "Scene/Entity.h". */
	std::string IncludePath;

	std::vector<FReflectedClass> Classes;

	bool operator==(const FReflectedHeader& Other) const = default;
};
