#pragma once

#include "CompiledShader.h"

#include <span>
#include <string>

/**
 * Writes the C++ source file that puts compiled shaders into the engine, so that the engine needs no shader files
 * beside it when it runs.
 *
 * The file defines FShaderLibrary::GetEmbeddedShaders(), which the engine declares in Rendering/ShaderLibrary.h, and
 * holds the SPIR-V of every shader as an array of numbers.
 */
class FShaderCodeGenerator
{
public:
	/**
	 * @param Shaders The shaders to embed, in the order they are to be listed. May be empty.
	 * @return The text of the source file. The same shaders always give the same text.
	 */
	static std::string Generate(std::span<const FCompiledShader> Shaders);
};
