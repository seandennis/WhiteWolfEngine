#pragma once

#include "ReflectedHeader.h"

#include <span>
#include <string>

/** Writes the C++ source that registers reflected classes with the engine. */
class FCodeGenerator
{
public:
	/**
	 * Returns the text of one source file that registers every class in Headers.
	 *
	 * For each class it defines StaticClass(), through WW_IMPLEMENT_CLASS, and RegisterClassMembers(), which adds
	 * one property per WPROPERTY() and one function per WFUNCTION(). Those are the two functions that the class
	 * declares with WW_DECLARE_CLASS. Headers without classes are left out. The same input always gives the same
	 * text.
	 */
	static std::string Generate(std::span<const FReflectedHeader> Headers);
};
