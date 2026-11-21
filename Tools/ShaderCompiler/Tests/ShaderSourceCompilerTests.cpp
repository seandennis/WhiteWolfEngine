#include "ShaderSourceCompiler.h"

#include "TestShaderFiles.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** What one call of FShaderSourceCompiler::Compile gave. */
	struct FCompileResult
	{
		bool bSucceeded = false;
		std::vector<uint32> Code;
		std::vector<std::string> Errors;
	};

	FCompileResult Compile(std::string_view Source,
		EShaderStage Stage,
		const std::filesystem::path& IncludeDirectory = std::filesystem::path("NoIncludeDirectory"))
	{
		const FShaderSourceCompiler Compiler;
		FCompileResult Result;
		Result.bSucceeded =
			Compiler.Compile(Source, "Shaders/Test.glsl", Stage, IncludeDirectory, Result.Code, Result.Errors);
		return Result;
	}

	bool AnyErrorContains(const FCompileResult& Result, std::string_view Text)
	{
		for (const std::string& Error : Result.Errors)
		{
			if (Error.find(Text) != std::string::npos)
			{
				return true;
			}
		}

		return false;
	}
}

TEST_SUITE("FShaderSourceCompiler")
{
	TEST_CASE("A vertex shader compiles to SPIR-V")
	{
		const FCompileResult Result = Compile(ValidVertexShader, EShaderStage::Vertex);

		CHECK(Result.bSucceeded);
		CHECK(Result.Errors.empty());
		REQUIRE_FALSE(Result.Code.empty());
		CHECK(Result.Code.front() == SpirvMagicNumber);
	}

	TEST_CASE("A fragment shader compiles to SPIR-V")
	{
		const FCompileResult Result = Compile(ValidFragmentShader, EShaderStage::Fragment);

		CHECK(Result.bSucceeded);
		CHECK(Result.Errors.empty());
		REQUIRE_FALSE(Result.Code.empty());
		CHECK(Result.Code.front() == SpirvMagicNumber);
	}

	TEST_CASE("The same source always compiles to the same code")
	{
		const FCompileResult First = Compile(ValidVertexShader, EShaderStage::Vertex);
		const FCompileResult Second = Compile(ValidVertexShader, EShaderStage::Vertex);

		REQUIRE(First.bSucceeded);
		CHECK(First.Code == Second.Code);
	}

	TEST_CASE("One compiler compiles any number of shaders")
	{
		const FShaderSourceCompiler Compiler;
		std::vector<uint32> Code;
		std::vector<std::string> Errors;

		CHECK(Compiler.Compile(ValidVertexShader, "A.vert", EShaderStage::Vertex, "None", Code, Errors));
		CHECK_FALSE(Compiler.Compile(
			"#version 460\nvoid main() { Broken; }\n", "B.frag", EShaderStage::Fragment, "None", Code, Errors));
		CHECK(Compiler.Compile(ValidFragmentShader, "C.frag", EShaderStage::Fragment, "None", Code, Errors));
		CHECK(Errors.empty());
		CHECK_FALSE(Code.empty());
	}

	TEST_CASE("An error names the source and the line, and no code is returned")
	{
		const FCompileResult Result = Compile("#version 460\n"
											  "void main()\n"
											  "{\n"
											  "\tgl_Position = vec4(Undeclared, 1.0);\n"
											  "}\n",
			EShaderStage::Vertex);

		CHECK_FALSE(Result.bSucceeded);
		CHECK(Result.Code.empty());
		REQUIRE_FALSE(Result.Errors.empty());
		CHECK(Result.Errors.front().starts_with("Shaders/Test.glsl(4): error: "));
		CHECK(AnyErrorContains(Result, "Undeclared"));
		// The compiler's closing count of the errors is not a problem of its own.
		CHECK_FALSE(AnyErrorContains(Result, "compilation errors"));
	}

	TEST_CASE("Only the first error of a shader is reported, because what follows it is often a consequence of it")
	{
		const FCompileResult Result = Compile("#version 460\n"
											  "void main()\n"
											  "{\n"
											  "\tFirstMistake = 1.0;\n"
											  "\tSecondMistake = 2.0;\n"
											  "}\n",
			EShaderStage::Vertex);

		CHECK_FALSE(Result.bSucceeded);
		REQUIRE(Result.Errors.size() == 1);
		CHECK(Result.Errors.front().starts_with("Shaders/Test.glsl(4): error: "));
	}

	TEST_CASE("A warning is an error")
	{
		const FCompileResult Result = Compile("#version 460\n"
											  "#extension GL_WW_no_such_extension : enable\n"
											  "void main()\n"
											  "{\n"
											  "\tgl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
											  "}\n",
			EShaderStage::Vertex);

		CHECK_FALSE(Result.bSucceeded);
		CHECK(Result.Code.empty());
		CHECK(AnyErrorContains(Result, "Shaders/Test.glsl(2): error: "));
		CHECK(AnyErrorContains(Result, "GL_WW_no_such_extension"));
	}

	TEST_CASE("A shader for the wrong stage is an error")
	{
		// A fragment shader cannot place a vertex.
		const FCompileResult Result = Compile(ValidVertexShader, EShaderStage::Fragment);

		CHECK_FALSE(Result.bSucceeded);
		CHECK(AnyErrorContains(Result, "gl_Position"));
	}

	TEST_CASE("A shader without a main function is an error")
	{
		const FCompileResult Result =
			Compile("#version 460\nfloat Half(float Value) { return Value * 0.5; }\n", EShaderStage::Vertex);

		CHECK_FALSE(Result.bSucceeded);
		CHECK(Result.Code.empty());
		CHECK(AnyErrorContains(Result, "Shaders/Test.glsl: error: "));
		CHECK(AnyErrorContains(Result, "entry point"));
	}

	TEST_CASE("Empty source is an error")
	{
		const FCompileResult Result = Compile("", EShaderStage::Vertex);

		CHECK_FALSE(Result.bSucceeded);
		CHECK(Result.Code.empty());
		CHECK_FALSE(Result.Errors.empty());
	}

	TEST_CASE("A shader includes files from the include directory")
	{
		const FScopedTempDirectory TempDirectory;
		WriteTestFile(
			TempDirectory.GetPath() / "Common" / "Colors.glsl", "const vec4 Orange = vec4(1.0, 0.5, 0.0, 1.0);\n");
		WriteTestFile(TempDirectory.GetPath() / "Output.glsl",
			"#include \"Common/Colors.glsl\"\n"
			"layout(location = 0) out vec4 Color;\n");

		const FCompileResult Result = Compile("#version 460\n"
											  "#include \"Output.glsl\"\n"
											  "void main()\n"
											  "{\n"
											  "\tColor = Orange;\n"
											  "}\n",
			EShaderStage::Fragment,
			TempDirectory.GetPath());

		CHECK(Result.bSucceeded);
		CHECK(Result.Errors.empty());
		CHECK_FALSE(Result.Code.empty());
	}

	TEST_CASE("Including a file that does not exist is an error on the line of the #include")
	{
		const FScopedTempDirectory TempDirectory;

		const FCompileResult Result = Compile("#version 460\n"
											  "#include \"Missing.glsl\"\n"
											  "void main()\n"
											  "{\n"
											  "}\n",
			EShaderStage::Vertex,
			TempDirectory.GetPath());

		CHECK_FALSE(Result.bSucceeded);
		CHECK(AnyErrorContains(Result, "Shaders/Test.glsl(2): error: "));
		CHECK(AnyErrorContains(Result, "Missing.glsl"));
	}

	TEST_CASE("An error in an included file names that file")
	{
		const FScopedTempDirectory TempDirectory;
		WriteTestFile(TempDirectory.GetPath() / "Broken.glsl", "\n\nconst float Value = NotDeclared;\n");

		const FCompileResult Result = Compile("#version 460\n"
											  "#include \"Broken.glsl\"\n"
											  "void main()\n"
											  "{\n"
											  "}\n",
			EShaderStage::Vertex,
			TempDirectory.GetPath());

		CHECK_FALSE(Result.bSucceeded);
		CHECK(AnyErrorContains(Result, "Broken.glsl(3): error: "));
		CHECK(AnyErrorContains(Result, "NotDeclared"));
	}

	TEST_CASE("A file that includes itself is an error, not an endless loop")
	{
		const FScopedTempDirectory TempDirectory;
		WriteTestFile(TempDirectory.GetPath() / "Loop.glsl", "#include \"Loop.glsl\"\n");

		const FCompileResult Result = Compile("#version 460\n"
											  "#include \"Loop.glsl\"\n"
											  "void main()\n"
											  "{\n"
											  "}\n",
			EShaderStage::Vertex,
			TempDirectory.GetPath());

		CHECK_FALSE(Result.bSucceeded);
		CHECK(AnyErrorContains(Result, "does a file include itself?"));
	}

	TEST_CASE("The stage of a shader file is told by its extension")
	{
		CHECK(FShaderSourceCompiler::GetStageOfFile("Unlit.vert") == EShaderStage::Vertex);
		CHECK(FShaderSourceCompiler::GetStageOfFile("Materials/Lit.frag") == EShaderStage::Fragment);
		CHECK(FShaderSourceCompiler::GetStageOfFile("Common.glsl") == std::nullopt);
		CHECK(FShaderSourceCompiler::GetStageOfFile("Unlit.vert.bak") == std::nullopt);
		CHECK(FShaderSourceCompiler::GetStageOfFile("vert") == std::nullopt);
		CHECK(FShaderSourceCompiler::GetStageOfFile("") == std::nullopt);
	}
}
