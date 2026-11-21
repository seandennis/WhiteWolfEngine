#pragma once

#include "CoreMinimal.h"

#include <span>
#include <string_view>

/** A shader that was compiled while the engine was built and is part of the engine DLL. */
struct FEmbeddedShader
{
	/** The path of the shader's source file below Engine/Shaders: "StaticMesh.vert". */
	std::string_view Name;

	/** The compiled shader, as SPIR-V words. */
	std::span<const uint32> Code;
};
