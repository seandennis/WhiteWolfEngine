#pragma once

#include "Core/CoreTypes.h"
#include "ShaderStage.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * Compiles GLSL shader source to SPIR-V for Vulkan 1.3.
 *
 * A shader is written in GLSL 4.60. It may pull in shared code with #include "File.glsl", which is looked up in
 * one include directory. Warnings are errors, as everywhere in the engine.
 *
 * The compiler library is started when the first object of this class is created and shut down with the last, so
 * keep one object for as long as shaders are being compiled.
 */
class FShaderSourceCompiler
{
public:
	FShaderSourceCompiler();
	~FShaderSourceCompiler();

	FShaderSourceCompiler(const FShaderSourceCompiler&) = delete;
	FShaderSourceCompiler& operator=(const FShaderSourceCompiler&) = delete;

	/** Returns the stage of a shader source file, told by its extension, or nothing if it is not a shader. */
	static std::optional<EShaderStage> GetStageOfFile(const std::filesystem::path& Path);

	/**
	 * Compiles one shader.
	 *
	 * @param Source The GLSL text.
	 * @param SourceName The name of the source in messages, usually its path.
	 * @param IncludeDirectory Where the files named in #include directives are found.
	 * @param OutCode Receives the SPIR-V words. Empty on failure.
	 * @param OutErrors Receives the problems, each as "name(line): error: text". Empty on success. The compiler
	 * stops at the first error in a shader, because the errors after it are often only consequences of it.
	 * @return True if the shader compiled without errors or warnings.
	 */
	bool Compile(std::string_view Source,
		const std::string& SourceName,
		EShaderStage Stage,
		const std::filesystem::path& IncludeDirectory,
		std::vector<uint32>& OutCode,
		std::vector<std::string>& OutErrors) const;
};
