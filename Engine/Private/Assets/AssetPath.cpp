#include "Assets/AssetPath.h"

#include <format>

namespace
{
	/** Checks the file part of an asset path. On failure OutProblem says what is wrong with it. */
	bool IsValidFile(std::string_view File, std::string& OutProblem)
	{
		if (File.empty())
		{
			OutProblem = "it names no file";
			return false;
		}
		if (File.find('\\') != std::string_view::npos)
		{
			OutProblem = "its parts must be separated by forward slashes";
			return false;
		}
		if (File.find(':') != std::string_view::npos)
		{
			OutProblem = "it must be relative to the content directory, without a drive";
			return false;
		}

		std::string_view Rest = File;
		while (true)
		{
			const std::size_t SlashPosition = Rest.find('/');
			const std::string_view Part = Rest.substr(0, SlashPosition);
			if (Part.empty())
			{
				OutProblem = File.starts_with('/') ? "it must be relative to the content directory"
												   : "it has an empty part between or after its slashes";
				return false;
			}
			if (Part == "." || Part == "..")
			{
				OutProblem = std::format("it must not have the part \"{}\"", Part);
				return false;
			}
			if (SlashPosition == std::string_view::npos)
			{
				return true;
			}
			Rest.remove_prefix(SlashPosition + 1);
		}
	}
}

std::optional<FAssetPath> FAssetPath::Parse(std::string_view Text, std::string& OutError)
{
	const std::size_t KeyPosition = Text.find('#');
	const std::string_view FileText = Text.substr(0, KeyPosition);
	const std::string_view KeyText =
		KeyPosition == std::string_view::npos ? std::string_view() : Text.substr(KeyPosition + 1);

	std::string Problem;
	if (!IsValidFile(FileText, Problem))
	{
		OutError = std::format("\"{}\" is not the path of an asset: {}", Text, Problem);
		return std::nullopt;
	}
	if (KeyText.find('#') != std::string_view::npos)
	{
		OutError = std::format("\"{}\" is not the path of an asset: it has more than one \"#\"", Text);
		return std::nullopt;
	}

	FAssetPath Path;
	Path.File = std::string(FileText);
	Path.Key = std::string(KeyText);
	return Path;
}

std::string FAssetPath::ToString() const
{
	return Key.empty() ? File : std::format("{}#{}", File, Key);
}
