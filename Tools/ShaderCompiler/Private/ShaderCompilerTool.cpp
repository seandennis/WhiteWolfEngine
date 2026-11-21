#include "ShaderCompilerTool.h"

#include "CompiledShader.h"
#include "ShaderCodeGenerator.h"
#include "ShaderSourceCompiler.h"
#include "ShaderStage.h"
#include "ToolFiles.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	constexpr int32 ExitSuccess = 0;
	constexpr int32 ExitShaderError = 1;
	constexpr int32 ExitUsageError = 2;

	/** The files the tool compiles. Other files below the root, such as the ones shaders include, are left alone. */
	constexpr std::string_view ShaderExtensions[] = {".vert", ".frag"};

	constexpr std::string_view Usage = "usage: WhiteWolfShaderCompiler --output <file> --root <dir>";

	struct FArguments
	{
		std::filesystem::path OutputFile;
		std::filesystem::path Root;
	};

	bool ParseArguments(std::span<const std::string> Arguments, FArguments& OutArguments, std::ostream& ErrorStream)
	{
		for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
		{
			const std::string& Option = Arguments[Index];
			const bool bIsKnownOption = Option == "--output" || Option == "--root";
			if (!bIsKnownOption)
			{
				ErrorStream << std::format("error: unknown argument '{}'\n{}\n", Option, Usage);
				return false;
			}
			if (Index + 1 == Arguments.size())
			{
				ErrorStream << std::format("error: {} needs a value\n{}\n", Option, Usage);
				return false;
			}

			++Index;
			std::filesystem::path& Value = Option == "--output" ? OutArguments.OutputFile : OutArguments.Root;
			if (!Value.empty())
			{
				ErrorStream << std::format("error: {} is given more than once\n{}\n", Option, Usage);
				return false;
			}
			Value = Arguments[Index];
		}

		if (OutArguments.OutputFile.empty() || OutArguments.Root.empty())
		{
			ErrorStream << std::format("error: --output and --root are required\n{}\n", Usage);
			return false;
		}

		return true;
	}
}

int32 FShaderCompilerTool::Run(std::span<const std::string> Arguments, std::ostream& ErrorStream)
{
	FArguments Options;
	if (!ParseArguments(Arguments, Options, ErrorStream))
	{
		return ExitUsageError;
	}

	std::vector<std::filesystem::path> ShaderPaths;
	std::string FindError;
	if (!FToolFiles::FindFiles(Options.Root, ShaderExtensions, ShaderPaths, FindError))
	{
		ErrorStream << std::format("error: {}\n", FindError);
		return ExitUsageError;
	}

	const FShaderSourceCompiler Compiler;
	std::vector<FCompiledShader> Shaders;
	bool bHasShaderErrors = false;

	for (const std::filesystem::path& ShaderPath : ShaderPaths)
	{
		std::string Source;
		if (!FToolFiles::ReadTextFile(ShaderPath, Source))
		{
			ErrorStream << std::format("error: cannot read '{}'\n", FToolFiles::ToUtf8(ShaderPath));
			return ExitUsageError;
		}

		// Only files with a shader's extension were looked for, so every one of them has a stage.
		const std::optional<EShaderStage> Stage = FShaderSourceCompiler::GetStageOfFile(ShaderPath);
		if (!Stage.has_value())
		{
			ErrorStream << std::format("error: '{}' is not a shader\n", FToolFiles::ToUtf8(ShaderPath));
			return ExitUsageError;
		}

		FCompiledShader Shader;
		Shader.Name = FToolFiles::ToUtf8(ShaderPath.lexically_relative(Options.Root));
		std::vector<std::string> Errors;
		if (!Compiler.Compile(Source, FToolFiles::ToUtf8(ShaderPath), *Stage, Options.Root, Shader.Code, Errors))
		{
			for (const std::string& Error : Errors)
			{
				ErrorStream << Error << '\n';
			}
			bHasShaderErrors = true;
			continue;
		}

		Shaders.push_back(std::move(Shader));
	}

	if (bHasShaderErrors)
	{
		return ExitShaderError;
	}

	if (!FToolFiles::WriteTextFileIfChanged(Options.OutputFile, FShaderCodeGenerator::Generate(Shaders)))
	{
		ErrorStream << std::format("error: cannot write '{}'\n", FToolFiles::ToUtf8(Options.OutputFile));
		return ExitUsageError;
	}

	return ExitSuccess;
}
