#pragma once

#include "CoreMinimal.h"

#include "Rendering/EmbeddedShader.h"

#include <span>
#include <string_view>

/**
 * The engine's shaders. They are written in GLSL in Engine/Shaders, compiled to SPIR-V by WhiteWolfShaderCompiler
 * while the engine is built, and embedded in the engine DLL, so they are always present and always match the code
 * that uses them.
 */
class FShaderLibrary
{
public:
	/**
	 * Returns the compiled code of a shader.
	 *
	 * @param Name The path of the shader's source file below Engine/Shaders: "StaticMesh.vert". Asking for a
	 * shader that does not exist is a programming error.
	 */
	static std::span<const uint32> GetCode(std::string_view Name);

private:
	/** Returns every embedded shader. Defined in the source file the shader compiler generates. */
	static std::span<const FEmbeddedShader> GetEmbeddedShaders();
};
