#include "ShaderSourceCompiler.h"

#include "ToolFiles.h"

#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/Logger.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <cstddef>
#include <format>
#include <memory>
#include <regex>
#include <sstream>
#include <utility>

static_assert(sizeof(unsigned int) == sizeof(uint32), "SPIR-V words are 32 bits wide");

namespace
{
	/** The GLSL version shaders are written in when they do not say. */
	constexpr int DefaultGlslVersion = 460;

	/** An #include may include further files, but a file that includes itself must stop somewhere. */
	constexpr std::size_t MaxIncludeDepth = 32;

	EShLanguage ToGlslangStage(EShaderStage Stage)
	{
		switch (Stage)
		{
			case EShaderStage::Fragment:
				return EShLangFragment;
			case EShaderStage::Vertex:
			default:
				return EShLangVertex;
		}
	}

	/**
	 * Resolves #include directives against one directory, and keeps what it has read until it is destroyed.
	 * #include "File" and #include <File> are looked up in the same way.
	 */
	class FDirectoryIncluder final : public glslang::TShader::Includer
	{
	public:
		explicit FDirectoryIncluder(std::filesystem::path InDirectory)
			: Directory(std::move(InDirectory))
		{
		}

		IncludeResult* includeLocal(const char* HeaderName, const char* /*IncluderName*/, std::size_t Depth) override
		{
			return Include(HeaderName, Depth);
		}

		IncludeResult* includeSystem(const char* HeaderName, const char* /*IncluderName*/, std::size_t Depth) override
		{
			return Include(HeaderName, Depth);
		}

		void releaseInclude(IncludeResult* /*Result*/) override
		{
			// Everything that was read is released together, with this object.
		}

	private:
		/** The text of an included file, or the reason it could not be included, with what the compiler is given. */
		struct FInclude
		{
			std::string Text;
			std::unique_ptr<IncludeResult> Result;
		};

		IncludeResult* Include(const char* HeaderName, std::size_t Depth)
		{
			const std::filesystem::path Path = Directory / HeaderName;

			std::unique_ptr<FInclude> Entry = std::make_unique<FInclude>();
			std::string ResolvedName;
			if (Depth > MaxIncludeDepth)
			{
				Entry->Text = std::format(
					"files are included more than {} levels deep; does a file include itself?", MaxIncludeDepth);
			}
			else if (FToolFiles::ReadTextFile(Path, Entry->Text))
			{
				ResolvedName = FToolFiles::ToUtf8(Path);
			}
			else
			{
				Entry->Text = std::format("cannot read '{}'", FToolFiles::ToUtf8(Path));
			}

			// The compiler takes a result without a name as a failure, and its text as the reason.
			Entry->Result =
				std::make_unique<IncludeResult>(ResolvedName, Entry->Text.data(), Entry->Text.size(), nullptr);

			IncludeResult* const Result = Entry->Result.get();
			Includes.push_back(std::move(Entry));
			return Result;
		}

		std::filesystem::path Directory;
		std::vector<std::unique_ptr<FInclude>> Includes;
	};

	/**
	 * Turns the compiler's log into messages of the form "name(line): error: text", the form that editors and the
	 * build's error matchers understand. Warnings are reported as errors too.
	 */
	void AppendMessages(const std::string& Log, const std::string& SourceName, std::vector<std::string>& OutErrors)
	{
		// The compiler writes "ERROR: name:line: text" for a problem in the source.
		static const std::regex LocatedMessage(R"(^(?:ERROR|WARNING): (.*?):(\d+): (.*)$)");
		static const std::regex OtherMessage(R"(^(?:ERROR|WARNING): (.*)$)");

		std::istringstream Lines(Log);
		std::string Line;
		while (std::getline(Lines, Line))
		{
			// The compiler ends its lines with a space, which is not part of the message.
			const std::size_t End = Line.find_last_not_of(" \r");
			Line.erase(End == std::string::npos ? 0 : End + 1);

			std::smatch Match;
			if (std::regex_match(Line, Match, LocatedMessage))
			{
				// After the first error the compiler stops, and says so as one more error on the same line.
				const bool bIsStopNotice = Match[3].str().ends_with("compilation terminated");
				if (!bIsStopNotice)
				{
					OutErrors.push_back(
						std::format("{}({}): error: {}", Match[1].str(), Match[2].str(), Match[3].str()));
				}
			}
			else if (std::regex_match(Line, Match, OtherMessage))
			{
				// The closing line only counts the errors above it.
				const bool bIsSummary = Match[1].str().find("compilation errors") != std::string::npos;
				if (!bIsSummary)
				{
					OutErrors.push_back(std::format("{}: error: {}", SourceName, Match[1].str()));
				}
			}
		}
	}
}

FShaderSourceCompiler::FShaderSourceCompiler()
{
	glslang::InitializeProcess();
}

FShaderSourceCompiler::~FShaderSourceCompiler()
{
	glslang::FinalizeProcess();
}

std::optional<EShaderStage> FShaderSourceCompiler::GetStageOfFile(const std::filesystem::path& Path)
{
	const std::filesystem::path Extension = Path.extension();
	if (Extension == ".vert")
	{
		return EShaderStage::Vertex;
	}
	if (Extension == ".frag")
	{
		return EShaderStage::Fragment;
	}

	return std::nullopt;
}

bool FShaderSourceCompiler::Compile(std::string_view Source,
	const std::string& SourceName,
	EShaderStage Stage,
	const std::filesystem::path& IncludeDirectory,
	std::vector<uint32>& OutCode,
	std::vector<std::string>& OutErrors) const
{
	OutCode.clear();
	OutErrors.clear();

	const EShLanguage Language = ToGlslangStage(Stage);
	const char* const SourceText = Source.data();
	const int SourceLength = static_cast<int>(Source.size());
	const char* const SourceNameText = SourceName.c_str();
	const EShMessages Messages = static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

	// The program must be destroyed before the shader it was linked from, hence the order of these two.
	glslang::TShader Shader(Language);
	glslang::TProgram Program;

	Shader.setStringsWithLengthsAndNames(&SourceText, &SourceLength, &SourceNameText, 1);
	Shader.setPreamble("#extension GL_GOOGLE_include_directive : require\n");
	Shader.setEntryPoint("main");
	Shader.setEnvInput(glslang::EShSourceGlsl, Language, glslang::EShClientVulkan, DefaultGlslVersion);
	Shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
	Shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

	FDirectoryIncluder Includer(IncludeDirectory);
	const bool bParsed = Shader.parse(GetDefaultResources(), DefaultGlslVersion, false, Messages, Includer);
	AppendMessages(Shader.getInfoLog(), SourceName, OutErrors);
	if (!bParsed || !OutErrors.empty())
	{
		if (OutErrors.empty())
		{
			OutErrors.push_back(std::format("{}: error: the shader could not be compiled", SourceName));
		}
		return false;
	}

	Program.addShader(&Shader);
	const bool bLinked = Program.link(Messages);
	AppendMessages(Program.getInfoLog(), SourceName, OutErrors);
	if (!bLinked || !OutErrors.empty())
	{
		if (OutErrors.empty())
		{
			OutErrors.push_back(std::format("{}: error: the shader could not be linked", SourceName));
		}
		return false;
	}

	std::vector<unsigned int> Words;
	spv::SpvBuildLogger Logger;
	glslang::GlslangToSpv(*Program.getIntermediate(Language), Words, &Logger);

	const std::string GeneratorMessages = Logger.getAllMessages();
	if (!GeneratorMessages.empty() || Words.empty())
	{
		OutErrors.push_back(std::format("{}: error: SPIR-V could not be generated. {}", SourceName, GeneratorMessages));
		return false;
	}

	OutCode.assign(Words.begin(), Words.end());
	return true;
}
