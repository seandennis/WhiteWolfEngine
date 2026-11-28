#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Rendering/HdrImage.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/TestGltfFiles.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

/**
 * An editor with a project in a temporary directory, and beside the project a directory of files that are not in
 * it yet, to import: what the tests of the asset commands share.
 */
struct FAssetCommandTest
{
	FAssetCommandTest()
	{
		std::filesystem::create_directories(GetSourceDirectory());
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Directory", FFileHelper::PathToUtf8(Directory.GetPath() / "Project"));
		Arguments.SetMember("Name", "Game");
		REQUIRE(Editor.ExecuteCommand("Project.New", Arguments).HasSucceeded());
	}

	std::filesystem::path GetSourceDirectory() const
	{
		return Directory.GetPath() / "Downloads";
	}

	std::filesystem::path GetContentDirectory() const
	{
		return Directory.GetPath() / "Project" / "Content";
	}

	/** Writes a PNG file into the directory of files to import, and returns where it is. */
	std::filesystem::path WritePicture(const std::string& Name) const
	{
		const std::filesystem::path Path = GetSourceDirectory() / FFileHelper::PathFromUtf8(Name);
		std::filesystem::create_directories(Path.parent_path());
		std::string Error;
		REQUIRE(FImageFile::SavePng(FImage(2, 2, FColor(255, 0, 0, 255)), Path, Error));
		return Path;
	}

	/** Writes a Radiance HDR file into the directory of files to import, and returns where it is. */
	std::filesystem::path WriteEnvironment(const std::string& Name) const
	{
		const std::filesystem::path Path = GetSourceDirectory() / FFileHelper::PathFromUtf8(Name);
		std::string Error;
		REQUIRE(FImageFile::SaveHdr(FHdrImage(4, 2, FLinearColor(1.0f, 1.0f, 1.0f)), Path, Error));
		return Path;
	}

	/** Writes a file of the given text into the directory of files to import, and returns where it is. */
	std::filesystem::path WriteText(const std::string& Name, std::string_view Text) const
	{
		const std::filesystem::path Path = GetSourceDirectory() / FFileHelper::PathFromUtf8(Name);
		WriteTestGltfFile(Path, Text);
		return Path;
	}

	/**
	 * Writes a material file into the directory of files to import, and returns where it is.
	 *
	 * @param Properties The JSON text of its "Properties".
	 */
	std::filesystem::path WriteMaterial(const std::string& Name, const std::string& Properties = "{}") const
	{
		return WriteText(Name, R"({"Format": "WhiteWolfMaterial", "Version": 1, "Properties": )" + Properties + "}");
	}

	/** Writes the test square as a .gltf file that holds everything itself, and returns where it is. */
	std::filesystem::path WriteSquare(const std::string& Name) const
	{
		return WriteText(Name, FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
	}

	/**
	 * Writes the test square as a .gltf file that keeps its buffer in "Square.bin" beside it and its one picture
	 * in "Textures/Paint.png" below it, and returns where the .gltf file is.
	 */
	std::filesystem::path WriteSquareWithExternalFiles(const std::string& Name) const
	{
		const FTestGltfBuffer Buffer = MakeTestSquareBuffer();
		WriteText(
			"Square.bin", std::string_view(reinterpret_cast<const char*>(Buffer.Bytes.data()), Buffer.Bytes.size()));
		WritePicture("Textures/Paint.png");

		FTestGltf Gltf;
		Gltf.More = R"("images": [{"uri": "Textures/Paint.png"}], "textures": [{"source": 0}], )";
		Gltf.Materials = R"([{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}])";
		return WriteText(
			Name, Gltf.Build(std::format(R"({{"byteLength": {}, "uri": "Square.bin"}})", Buffer.Bytes.size())));
	}

	/** Gives the editor Asset.Import for a file, into a folder of the content directory or its top. */
	FCommandResult Import(const std::filesystem::path& Source, const std::string& Folder = std::string())
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Source", FFileHelper::PathToUtf8(Source));
		if (!Folder.empty())
		{
			Arguments.SetMember("Folder", Folder);
		}
		return Editor.ExecuteCommand("Asset.Import", Arguments);
	}

	/** Gives the editor a command with the arguments written as JSON text. */
	FCommandResult Execute(std::string_view CommandName, std::string_view ArgumentText = "{}")
	{
		return Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
	}

	FScopedEditorTest EditorTest;
	FScopedTempDirectory Directory;
	FEditor Editor;
};
