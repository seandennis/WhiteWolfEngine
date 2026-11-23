#pragma once

#include "CoreMinimal.h"

#include "Rendering/EmbeddedShader.h"

#include <nvrhi/nvrhi.h>

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

	/**
	 * Creates a shader of the engine on a graphics card.
	 *
	 * @param Name As for GetCode.
	 * @param Type What kind of shader the file is: its extension says so, .vert or .frag.
	 * @return The shader, or null if the graphics card did not accept it.
	 */
	static nvrhi::ShaderHandle CreateShader(nvrhi::IDevice& Device, std::string_view Name, nvrhi::ShaderType Type);

private:
	/** Returns every embedded shader. Defined in the source file the shader compiler generates. */
	static std::span<const FEmbeddedShader> GetEmbeddedShaders();
};
