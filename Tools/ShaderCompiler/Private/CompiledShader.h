#pragma once

#include "Core/CoreTypes.h"

#include <string>
#include <vector>

/** A shader after compilation. */
struct FCompiledShader
{
	/** The file name of the source, which is how the engine asks for the shader: "Unlit.vert". */
	std::string Name;

	/** The SPIR-V code, as the 32-bit words Vulkan takes. */
	std::vector<uint32> Code;

	bool operator==(const FCompiledShader& Other) const = default;
};
