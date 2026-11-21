#include "ToolFiles.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>

std::string FToolFiles::ToUtf8(const std::filesystem::path& Path)
{
	const std::u8string Text = Path.generic_u8string();
	return std::string(Text.begin(), Text.end());
}

bool FToolFiles::FindFiles(const std::filesystem::path& Root,
	std::span<const std::string_view> Extensions,
	std::vector<std::filesystem::path>& OutFiles,
	std::string& OutError)
{
	OutFiles.clear();
	OutError.clear();

	std::error_code Error;
	std::filesystem::recursive_directory_iterator Iterator(Root, Error);
	const std::filesystem::recursive_directory_iterator End;
	while (!Error && Iterator != End)
	{
		const std::filesystem::directory_entry& Entry = *Iterator;
		const std::string Extension = ToUtf8(Entry.path().extension());
		const bool bHasWantedExtension = std::ranges::find(Extensions, Extension) != Extensions.end();
		std::error_code EntryError;
		if (bHasWantedExtension && Entry.is_regular_file(EntryError))
		{
			OutFiles.push_back(Entry.path());
		}

		Iterator.increment(Error);
	}

	if (Error)
	{
		OutFiles.clear();
		OutError = std::format("cannot read the directory '{}': {}", ToUtf8(Root), Error.message());
		return false;
	}

	std::ranges::sort(OutFiles,
		[](const std::filesystem::path& Left, const std::filesystem::path& Right)
		{ return Left.generic_u8string() < Right.generic_u8string(); });
	return true;
}

bool FToolFiles::ReadTextFile(const std::filesystem::path& Path, std::string& OutText)
{
	OutText.clear();

	std::ifstream Stream(Path, std::ios::binary);
	if (!Stream)
	{
		return false;
	}

	OutText.assign(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
	return !Stream.bad();
}

bool FToolFiles::WriteTextFileIfChanged(const std::filesystem::path& Path, std::string_view Text)
{
	std::string ExistingText;
	if (ReadTextFile(Path, ExistingText) && ExistingText == Text)
	{
		return true;
	}

	std::error_code Error;
	std::filesystem::create_directories(Path.parent_path(), Error);
	if (Error)
	{
		return false;
	}

	std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
	Stream.write(Text.data(), static_cast<std::streamsize>(Text.size()));
	Stream.flush();
	return Stream.good();
}
