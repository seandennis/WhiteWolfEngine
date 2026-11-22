#include "Core/FileHelper.h"

#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

bool FFileHelper::LoadFileToString(const std::filesystem::path& Path, std::string& OutText, std::string& OutError)
{
	// Binary mode: the bytes of the file are returned as they are, whatever its line endings.
	std::ifstream Stream(Path, std::ios::in | std::ios::binary);
	if (!Stream.is_open())
	{
		OutError = std::format("cannot open '{}' for reading", PathToUtf8(Path));
		return false;
	}

	std::string Text((std::istreambuf_iterator<char>(Stream)), std::istreambuf_iterator<char>());
	if (Stream.bad())
	{
		OutError = std::format("cannot read '{}'", PathToUtf8(Path));
		return false;
	}

	OutText = std::move(Text);
	return true;
}

bool FFileHelper::SaveStringToFile(std::string_view Text, const std::filesystem::path& Path, std::string& OutError)
{
	std::error_code Error;
	const std::filesystem::path Directory = Path.parent_path();
	if (!Directory.empty())
	{
		std::filesystem::create_directories(Directory, Error);
		if (Error)
		{
			OutError = std::format("cannot create the directory '{}': {}", PathToUtf8(Directory), Error.message());
			return false;
		}
	}

	std::filesystem::path TemporaryPath = Path;
	TemporaryPath += ".tmp";

	bool bWritten = false;
	{
		std::ofstream Stream(TemporaryPath, std::ios::out | std::ios::trunc | std::ios::binary);
		if (Stream.is_open())
		{
			Stream.write(Text.data(), static_cast<std::streamsize>(Text.size()));
			Stream.flush();
			bWritten = Stream.good();
		}
	}

	if (!bWritten)
	{
		std::filesystem::remove(TemporaryPath, Error);
		OutError = std::format("cannot write '{}'", PathToUtf8(Path));
		return false;
	}

	std::filesystem::rename(TemporaryPath, Path, Error);
	if (Error)
	{
		const std::string Reason = Error.message();
		std::filesystem::remove(TemporaryPath, Error);
		OutError = std::format("cannot replace '{}': {}", PathToUtf8(Path), Reason);
		return false;
	}

	return true;
}

std::string FFileHelper::PathToUtf8(const std::filesystem::path& Path)
{
	const std::u8string Text = Path.u8string();
	return std::string(Text.begin(), Text.end());
}

std::filesystem::path FFileHelper::PathFromUtf8(std::string_view Text)
{
	return std::filesystem::path(std::u8string(Text.begin(), Text.end()));
}
