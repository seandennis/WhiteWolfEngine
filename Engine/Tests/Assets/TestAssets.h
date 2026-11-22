#pragma once

#include "CoreMinimal.h"

#include "Assets/AssetManager.h"
#include "Core/FileHelper.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/HdrImage.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/TestGltfFiles.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

/**
 * A content directory for one test: a temporary directory that asset paths are relative to while this object is
 * alive. Garbage is collected before and after, so that no package of another test is still loaded.
 */
class FScopedContentDirectory
{
public:
	FScopedContentDirectory()
	{
		CollectGarbage();
		FAssetManager::SetContentDirectory(Directory.GetPath());
	}

	~FScopedContentDirectory()
	{
		FAssetManager::SetContentDirectory(std::filesystem::path());
		CollectGarbage();
	}

	FScopedContentDirectory(const FScopedContentDirectory&) = delete;
	FScopedContentDirectory& operator=(const FScopedContentDirectory&) = delete;

	const std::filesystem::path& GetPath() const
	{
		return Directory.GetPath();
	}

	/** Writes a PNG file of one colour, two pixels by two, into the directory. */
	void WritePicture(const std::string& File, FColor Color = FColor(255, 0, 0, 255)) const
	{
		const FImage Image(2, 2, Color);
		std::string Error;

		const bool bSaved = FImageFile::SavePng(Image, MakeFilePath(File), Error);
		INFO("Error: ", Error);
		REQUIRE(bSaved);
	}

	/** Writes a Radiance HDR file of one colour, four pixels by two, into the directory. */
	void WriteEnvironment(const std::string& File) const
	{
		const FHdrImage Image(4, 2, FLinearColor(2.0f, 1.0f, 0.5f));
		std::string Error;

		const bool bSaved = FImageFile::SaveHdr(Image, MakeFilePath(File), Error);
		INFO("Error: ", Error);
		REQUIRE(bSaved);
	}

	/** Writes the glTF file of the test square, one mesh with one material, into the directory. */
	void WriteSquare(const std::string& File) const
	{
		WriteText(File, FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
	}

	/** Writes a file of the given text into the directory. */
	void WriteText(const std::string& File, const std::string& Text) const
	{
		WriteTestGltfFile(GetPath() / FFileHelper::PathFromUtf8(File), Text);
	}

private:
	/** Returns where a file of the directory is, and makes sure the directory it is in exists. */
	std::filesystem::path MakeFilePath(const std::string& File) const
	{
		std::filesystem::path Path = GetPath() / FFileHelper::PathFromUtf8(File);
		std::filesystem::create_directories(Path.parent_path());
		return Path;
	}

	FScopedTempDirectory Directory;
};

/**
 * Returns how many warnings and errors have been logged. A test that loads assets counts these instead of all
 * messages, because the asset manager logs a line for every file it loads.
 */
inline int32 CountLoggedProblems(const FScopedLogCapture& Capture)
{
	int32 Count = 0;
	for (const FLogMessage& Message : Capture.GetMessages())
	{
		if (Message.Verbosity <= ELogVerbosity::Warning)
		{
			++Count;
		}
	}
	return Count;
}
