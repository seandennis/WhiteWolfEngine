#pragma once

#include "CoreMinimal.h"

#include "Core/Base64.h"
#include "Core/FileHelper.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Builds small glTF files for the tests of FGltfImporter, so that the tests need no binary files in the
// repository and say in their own text what the file under test contains.

/** The bytes of a glTF buffer, built by appending numbers. Numbers are stored little-endian, as glTF requires. */
class FTestGltfBuffer
{
public:
	template <typename NumberType>
	FTestGltfBuffer& Append(std::initializer_list<NumberType> Numbers)
	{
		for (const NumberType Number : Numbers)
		{
			std::byte Raw[sizeof(NumberType)];
			std::memcpy(Raw, &Number, sizeof(NumberType));
			Bytes.insert(Bytes.end(), std::begin(Raw), std::end(Raw));
		}
		return *this;
	}

	FTestGltfBuffer& AppendBytes(std::span<const std::byte> MoreBytes)
	{
		Bytes.insert(Bytes.end(), MoreBytes.begin(), MoreBytes.end());
		return *this;
	}

	std::vector<std::byte> Bytes;
};

/** Returns the address that embeds bytes in a glTF file. */
inline std::string MakeTestDataUri(
	std::span<const std::byte> Bytes, std::string_view MimeType = "application/octet-stream")
{
	return std::string("data:") + std::string(MimeType) + ";base64," + FBase64::Encode(Bytes);
}

/**
 * The buffer of the test square: one metre a side in the XY plane, facing +Z, with a texture lying on it like a
 * picture on a wall.
 *
 *     bytes   0 to  47   positions, 4 times three floats
 *     bytes  48 to  95   normals, 4 times three floats
 *     bytes  96 to 127   texture coordinates, 4 times two floats
 *     bytes 128 to 139   indices of two triangles, 6 unsigned 16-bit numbers
 */
inline FTestGltfBuffer MakeTestSquareBuffer()
{
	FTestGltfBuffer Buffer;
	Buffer.Append<float>({-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f, -0.5f, 0.5f, 0.0f});
	Buffer.Append<float>({0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f});
	Buffer.Append<float>({0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f});
	Buffer.Append<uint16>({0, 1, 2, 0, 2, 3});
	return Buffer;
}

/**
 * The parts of a glTF file, as JSON text, with defaults that describe the test square: one node with one mesh
 * with one material. A test replaces the parts it is about.
 */
struct FTestGltf
{
	std::string Nodes = R"([{"name": "Square", "mesh": 0}])";
	std::string Scenes = R"([{"nodes": [0]}])";
	std::string Meshes = R"([{"name": "SquareMesh", "primitives": [
		{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}])";
	std::string Materials = R"([{"name": "Paint"}])";
	std::string Accessors = R"([
		{"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 0]},
		{"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
		{"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
		{"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"}])";
	std::string BufferViews = R"([
		{"buffer": 0, "byteOffset": 0, "byteLength": 48},
		{"buffer": 0, "byteOffset": 48, "byteLength": 48},
		{"buffer": 0, "byteOffset": 96, "byteLength": 32},
		{"buffer": 0, "byteOffset": 128, "byteLength": 12}])";

	/** Further members of the file's top-level object, each followed by a comma: "images": [...], and so on. */
	std::string More;

	/** The version the file claims to be. */
	std::string Version = "2.0";

	/**
	 * Returns the text of the .gltf file.
	 *
	 * @param BufferJson The file's one buffer, as a JSON object: {"byteLength": 140, "uri": "..."}.
	 */
	std::string Build(const std::string& BufferJson) const
	{
		return "{\"asset\": {\"version\": \"" + Version + "\"}, " + More + "\"scene\": 0, \"scenes\": " + Scenes +
			", \"nodes\": " + Nodes + ", \"meshes\": " + Meshes + ", \"materials\": " + Materials +
			", \"accessors\": " + Accessors + ", \"bufferViews\": " + BufferViews + ", \"buffers\": [" + BufferJson +
			"]}";
	}

	/** Returns the text of a .gltf file that has the given buffer written into it. */
	std::string BuildWithEmbeddedBuffer(const FTestGltfBuffer& Buffer) const
	{
		return Build("{\"byteLength\": " + std::to_string(Buffer.Bytes.size()) + ", \"uri\": \"" +
			MakeTestDataUri(Buffer.Bytes) + "\"}");
	}

	/** Returns the bytes of a .glb file that carries the given buffer as its binary part. */
	std::string BuildBinaryFile(const FTestGltfBuffer& Buffer) const
	{
		// Each part of a .glb file is padded to a multiple of four bytes: the text with spaces, the data with zeros.
		std::string Json = Build("{\"byteLength\": " + std::to_string(Buffer.Bytes.size()) + "}");
		Json.resize((Json.size() + 3) / 4 * 4, ' ');
		std::string Binary(reinterpret_cast<const char*>(Buffer.Bytes.data()), Buffer.Bytes.size());
		Binary.resize((Binary.size() + 3) / 4 * 4, '\0');

		const auto AppendNumber = [](std::string& Text, uint32 Number)
		{
			char Raw[sizeof(uint32)];
			std::memcpy(Raw, &Number, sizeof(uint32));
			Text.append(Raw, sizeof(uint32));
		};

		constexpr uint32 FileMagic = 0x46546C67;
		constexpr uint32 JsonChunkType = 0x4E4F534A;
		constexpr uint32 BinaryChunkType = 0x004E4942;
		constexpr uint32 HeaderSize = 12;
		constexpr uint32 ChunkHeaderSize = 8;

		std::string File;
		AppendNumber(File, FileMagic);
		AppendNumber(File, 2);
		AppendNumber(
			File, static_cast<uint32>(HeaderSize + ChunkHeaderSize + Json.size() + ChunkHeaderSize + Binary.size()));
		AppendNumber(File, static_cast<uint32>(Json.size()));
		AppendNumber(File, JsonChunkType);
		File += Json;
		AppendNumber(File, static_cast<uint32>(Binary.size()));
		AppendNumber(File, BinaryChunkType);
		File += Binary;
		return File;
	}
};

/** Writes a file for a test, which a test expects to work. */
inline void WriteTestGltfFile(const std::filesystem::path& Path, std::string_view Content)
{
	std::string Error;
	const bool bSaved = FFileHelper::SaveStringToFile(Content, Path, Error);
	INFO("Error: ", Error);
	REQUIRE(bSaved);
}
