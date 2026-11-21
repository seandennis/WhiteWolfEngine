#include "Rendering/GltfImporter.h"

#include "Logging/LogMessage.h"
#include "Math/Color.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/Material.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestGltfFiles.h"
#include "Rendering/TestSceneRendering.h"
#include "Rendering/Texture.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** A scene, a directory for the files of one test, and what the last import reported. */
	struct FGltfImportTest
	{
		/** Writes a file into the test's directory and imports it. */
		WEntity* Import(const std::string& FileName, std::string_view Content)
		{
			WriteTestGltfFile(TempDirectory.GetPath() / FileName, Content);
			return ImportFile(FileName);
		}

		/** Imports a file that is already in the test's directory. */
		WEntity* ImportFile(const std::string& FileName)
		{
			Error.clear();
			Warnings.clear();
			return FGltfImporter::Import(TempDirectory.GetPath() / FileName, *Scene.Get(), Error, &Warnings);
		}

		/** Imports a file that the test expects to be valid, and returns the entity of its only node. */
		WEntity* ImportSingleNode(const std::string& FileName, std::string_view Content)
		{
			const WEntity* const Root = Import(FileName, Content);
			INFO("Error: ", Error);
			REQUIRE(Root != nullptr);
			REQUIRE(Root->GetChildren().size() == 1);
			return Root->GetChildren().front().Get();
		}

		bool HasWarning(std::string_view Text) const
		{
			for (const std::string& Warning : Warnings)
			{
				if (Warning.find(Text) != std::string::npos)
				{
					return true;
				}
			}
			return false;
		}

		const FScopedLogCapture Capture;
		const FScopedSceneTest SceneTest;
		const FScopedTempDirectory TempDirectory;
		const FScopedTestScene Scene;
		std::string Error;
		std::vector<std::string> Warnings;
	};

	std::vector<WStaticMeshComponent*> GetMeshComponents(const WEntity& Entity)
	{
		std::vector<WStaticMeshComponent*> Components;
		for (const TObjectPtr<WComponent>& Component : Entity.GetComponents())
		{
			if (WStaticMeshComponent* const MeshComponent = Cast<WStaticMeshComponent>(Component.Get()))
			{
				Components.push_back(MeshComponent);
			}
		}
		return Components;
	}

	/** Returns the only mesh component of an entity, which a test expects it to have. */
	WStaticMeshComponent* GetOnlyMeshComponent(const WEntity& Entity)
	{
		const std::vector<WStaticMeshComponent*> Components = GetMeshComponents(Entity);
		REQUIRE(Components.size() == 1);
		REQUIRE(Components.front()->Mesh != nullptr);
		return Components.front();
	}

	bool IsNearVector(const FVector3f& Actual, const FVector3f& Expected, float Tolerance = 1.0e-5f)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	/** The bytes of a small PNG file whose four quarters are red, green, blue and white. */
	std::vector<std::byte> MakeTestPng()
	{
		FImage Image(16, 16);
		for (int32 Y = 0; Y < 16; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				Image.SetPixel(X, Y, Y < 8 ? (X < 8 ? Red : Green) : (X < 8 ? Blue : White));
			}
		}

		std::vector<std::byte> FileData;
		std::string Error;
		REQUIRE(FImageFile::SavePngToMemory(Image, FileData, Error));
		return FileData;
	}

	std::string_view AsText(const std::vector<std::byte>& Bytes)
	{
		return std::string_view(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());
	}

	/** The material of the test square, with all of its textures taken from one image file beside the glTF file. */
	constexpr std::string_view TexturedMaterial = R"([{"name": "Paint",
		"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicRoughnessTexture": {"index": 0}},
		"normalTexture": {"index": 0, "scale": 0.5}, "emissiveTexture": {"index": 0},
		"occlusionTexture": {"index": 0, "strength": 0.75}}])";
}

TEST_SUITE("FGltfImporter")
{
	TEST_CASE("A file becomes an entity named after it, with the file's nodes below it")
	{
		FGltfImportTest Test;

		const WEntity* const Root =
			Test.Import("Garden Shed.gltf", FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		INFO("Error: ", Test.Error);
		REQUIRE(Root != nullptr);
		CHECK(Root->GetName() == "Garden Shed");
		CHECK(Root->GetScene() == Test.Scene.Get());
		CHECK(Root->GetRelativeTransform() == FTransform());
		REQUIRE(Test.Scene->GetRootEntities().size() == 1);
		CHECK(Test.Scene->GetRootEntities().front().Get() == Root);
		REQUIRE(Root->GetChildren().size() == 1);
		CHECK(Root->GetChildren().front()->GetName() == "Square");
		CHECK(Test.Warnings.empty());
		CHECK(Test.Error.empty());
	}

	TEST_CASE("A mesh arrives with its positions, normals, texture coordinates and triangles")
	{
		FGltfImportTest Test;

		const WEntity* const Square =
			Test.ImportSingleNode("Square.gltf", FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		const WStaticMesh* const Mesh = GetOnlyMeshComponent(*Square)->Mesh.Get();
		const std::vector<FStaticMeshVertex> ExpectedVertices{
			{FVector3f(-0.5f, -0.5f, 0.0f), AxisZ, FVector2f(0.0f, 1.0f)},
			{FVector3f(0.5f, -0.5f, 0.0f), AxisZ, FVector2f(1.0f, 1.0f)},
			{FVector3f(0.5f, 0.5f, 0.0f), AxisZ, FVector2f(1.0f, 0.0f)},
			{FVector3f(-0.5f, 0.5f, 0.0f), AxisZ, FVector2f(0.0f, 0.0f)},
		};
		CHECK(Mesh->GetVertices() == ExpectedVertices);
		CHECK(Mesh->GetIndices() == std::vector<uint32>{0, 1, 2, 0, 2, 3});
	}

	TEST_CASE("The three ways a file can keep its data give the same mesh")
	{
		const FTestGltfBuffer Buffer = MakeTestSquareBuffer();
		const FTestGltf Gltf;

		FGltfImportTest Test;
		const WEntity* const Embedded = Test.ImportSingleNode("Embedded.gltf", Gltf.BuildWithEmbeddedBuffer(Buffer));

		// A .bin file beside the .gltf file, in a subdirectory and with a space in its name.
		WriteTestGltfFile(Test.TempDirectory.GetPath() / "Data" / "Square Data.bin", AsText(Buffer.Bytes));
		const WEntity* const External = Test.ImportSingleNode(
			"External.gltf", Gltf.Build(R"({"byteLength": 140, "uri": "Data/Square%20Data.bin"})"));

		// Everything in one binary file.
		const WEntity* const Binary = Test.ImportSingleNode("Binary.glb", Gltf.BuildBinaryFile(Buffer));

		const WStaticMesh* const Expected = GetOnlyMeshComponent(*Embedded)->Mesh.Get();
		for (const WEntity* const Entity : {External, Binary})
		{
			const WStaticMesh* const Mesh = GetOnlyMeshComponent(*Entity)->Mesh.Get();
			CHECK(Mesh->GetVertices() == Expected->GetVertices());
			CHECK(Mesh->GetIndices() == Expected->GetIndices());
		}
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("Nodes keep their names, order, hierarchy and transforms")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Scenes = R"([{"nodes": [0, 3]}])";
		Gltf.Nodes = R"([
			{"name": "Parent", "translation": [1, 2, 3], "rotation": [0, 0.7071068, 0, 0.7071068], "scale": [2, 2, 2],
			 "children": [1, 2]},
			{"name": "First", "mesh": 0, "translation": [0.5, 0, 0]},
			{"mesh": 0},
			{"name": "Matrix", "matrix": [0, 0, -3, 0,  0, 2, 0, 0,  1, 0, 0, 0,  10, 20, 30, 1]}])";

		const WEntity* const Root = Test.Import("Tree.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		INFO("Error: ", Test.Error);
		REQUIRE(Root != nullptr);
		REQUIRE(Root->GetChildren().size() == 2);
		const WEntity* const Parent = Root->GetChildren()[0].Get();
		const WEntity* const Matrix = Root->GetChildren()[1].Get();

		CHECK(Parent->GetName() == "Parent");
		CHECK(Parent->GetRelativeTransform().Location == FVector(1.0, 2.0, 3.0));
		CHECK(Parent->GetRelativeTransform().Scale == FVector3f(2.0f));
		// A quarter turn to the left about the vertical axis: what faced -Z now faces -X.
		CHECK(IsNearVector(
			Parent->GetRelativeTransform().Rotation * FVector3f(0.0f, 0.0f, -1.0f), FVector3f(-1.0f, 0.0f, 0.0f)));

		REQUIRE(Parent->GetChildren().size() == 2);
		CHECK(Parent->GetChildren()[0]->GetName() == "First");
		CHECK(Parent->GetChildren()[0]->GetRelativeTransform().Location == FVector(0.5, 0.0, 0.0));
		// A node without a name is named after its number in the file.
		CHECK(Parent->GetChildren()[1]->GetName() == "Node2");
		// The child's place in the world follows from its parent's transform.
		CHECK(glm::length(Parent->GetChildren()[0]->GetWorldLocation() - FVector(1.0, 2.0, 2.0)) < 1.0e-5);

		// The matrix moves by (10, 20, 30), turns a quarter turn to the right and scales by (3, 2, 1).
		CHECK(Matrix->GetName() == "Matrix");
		const FTransform& Transform = Matrix->GetRelativeTransform();
		CHECK(Transform.Location == FVector(10.0, 20.0, 30.0));
		CHECK(IsNearVector(Transform.Scale, FVector3f(3.0f, 2.0f, 1.0f)));
		CHECK(IsNearVector(Transform.Rotation * FVector3f(1.0f, 0.0f, 0.0f), FVector3f(0.0f, 0.0f, -1.0f)));
		CHECK(IsNearVector(Transform.Rotation * FVector3f(0.0f, 0.0f, 1.0f), FVector3f(1.0f, 0.0f, 0.0f)));
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("A matrix that mirrors is imported as a transform that mirrors")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Nodes =
			R"([{"name": "Mirror", "mesh": 0, "matrix": [-1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1]}])";

		const WEntity* const Mirror =
			Test.ImportSingleNode("Mirror.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		// Whatever the split into rotation and scale, a point on the right must end up on the left.
		const FVector Mirrored = Mirror->GetRelativeTransform().TransformPosition(FVector(1.0, 2.0, 3.0));
		CHECK(glm::length(Mirrored - FVector(-1.0, 2.0, 3.0)) < 1.0e-5);
	}

	TEST_CASE("Only the nodes of the scene the file shows are imported")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Nodes =
			R"([{"name": "InFirstScene", "mesh": 0}, {"name": "InSecondScene", "mesh": 0}, {"name": "InNoScene"}])";
		Gltf.Scenes = R"([{"nodes": [0]}, {"nodes": [1]}])";
		std::string Content = Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer());
		const std::string_view DefaultScene = "\"scene\": 0";
		Content.replace(Content.find(DefaultScene), DefaultScene.size(), "\"scene\": 1");

		const WEntity* const Node = Test.ImportSingleNode("Scenes.gltf", Content);

		CHECK(Node->GetName() == "InSecondScene");
	}

	TEST_CASE("What a file uses several times is created once and shared")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Scenes = R"([{"nodes": [0, 1]}])";
		Gltf.Nodes = R"([{"name": "A", "mesh": 0}, {"name": "B", "mesh": 0}])";

		const WEntity* const Root = Test.Import("Shared.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		REQUIRE(Root != nullptr);
		REQUIRE(Root->GetChildren().size() == 2);
		const WStaticMeshComponent* const First = GetOnlyMeshComponent(*Root->GetChildren()[0]);
		const WStaticMeshComponent* const Second = GetOnlyMeshComponent(*Root->GetChildren()[1]);
		CHECK(First->Mesh.Get() == Second->Mesh.Get());
		REQUIRE(First->Material != nullptr);
		CHECK(First->Material.Get() == Second->Material.Get());
	}

	TEST_CASE("A mesh of several parts becomes several components, each with its part's material")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Meshes = R"([{"primitives": [
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 0},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 1},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3}]}])";
		Gltf.Materials = R"([{"name": "Red", "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1]}},
			{"name": "Blue", "pbrMetallicRoughness": {"baseColorFactor": [0, 0, 1, 1]}}])";

		const WEntity* const Node =
			Test.ImportSingleNode("Parts.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		const std::vector<WStaticMeshComponent*> Components = GetMeshComponents(*Node);
		REQUIRE(Components.size() == 3);
		REQUIRE(Components[0]->Material != nullptr);
		REQUIRE(Components[1]->Material != nullptr);
		CHECK(Components[0]->Material->BaseColor == FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		CHECK(Components[1]->Material->BaseColor == FLinearColor(0.0f, 0.0f, 1.0f, 1.0f));
		// A part without a material has none here either, and is drawn with the engine's default.
		CHECK(Components[2]->Material == nullptr);
		// The parts are different meshes even where they happen to hold the same triangles.
		CHECK(Components[0]->Mesh.Get() != Components[1]->Mesh.Get());
	}

	TEST_CASE("A mesh without normals gets flat ones, and one without texture coordinates gets zeros")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Meshes = R"([{"primitives": [{"attributes": {"POSITION": 0}, "indices": 3}]}])";

		const WEntity* const Node =
			Test.ImportSingleNode("Flat.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		// Each triangle has its own three vertices, so that it can have its own normal.
		const WStaticMesh* const Mesh = GetOnlyMeshComponent(*Node)->Mesh.Get();
		REQUIRE(Mesh->GetVertices().size() == 6);
		CHECK(Mesh->GetIndices() == std::vector<uint32>{0, 1, 2, 3, 4, 5});
		for (const FStaticMeshVertex& Vertex : Mesh->GetVertices())
		{
			CHECK(IsNearVector(Vertex.Normal, AxisZ));
			CHECK(Vertex.TextureCoordinate == FVector2f(0.0f));
		}
		CHECK(Mesh->GetVertices()[3].Position == FVector3f(-0.5f, -0.5f, 0.0f));
		CHECK(Mesh->GetVertices()[5].Position == FVector3f(-0.5f, 0.5f, 0.0f));
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("Indices of every size are read, and a mesh without indices is a plain list of triangles")
	{
		FGltfImportTest Test;

		// The same two triangles with 8-bit and 32-bit indices appended to the buffer.
		FTestGltfBuffer Buffer = MakeTestSquareBuffer();
		Buffer.Append<uint8>({0, 1, 2, 0, 2, 3});
		Buffer.Append<uint8>({0, 0});
		Buffer.Append<uint32>({0, 1, 2, 0, 2, 3});
		FTestGltf Gltf;
		Gltf.BufferViews = R"([
			{"buffer": 0, "byteOffset": 0, "byteLength": 48},
			{"buffer": 0, "byteOffset": 48, "byteLength": 48},
			{"buffer": 0, "byteOffset": 96, "byteLength": 32},
			{"buffer": 0, "byteOffset": 128, "byteLength": 12},
			{"buffer": 0, "byteOffset": 140, "byteLength": 6},
			{"buffer": 0, "byteOffset": 148, "byteLength": 24}])";
		Gltf.Accessors = R"([
			{"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 0]},
			{"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
			{"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
			{"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
			{"bufferView": 4, "componentType": 5121, "count": 6, "type": "SCALAR"},
			{"bufferView": 5, "componentType": 5125, "count": 6, "type": "SCALAR"},
			{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 0]},
			{"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"}])";
		Gltf.Meshes = R"([{"primitives": [
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 4},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 5},
			{"attributes": {"POSITION": 6, "NORMAL": 7}}]}])";

		const WEntity* const Node = Test.ImportSingleNode("Indices.gltf", Gltf.BuildWithEmbeddedBuffer(Buffer));

		const std::vector<WStaticMeshComponent*> Components = GetMeshComponents(*Node);
		REQUIRE(Components.size() == 3);
		CHECK(Components[0]->Mesh->GetIndices() == std::vector<uint32>{0, 1, 2, 0, 2, 3});
		CHECK(Components[1]->Mesh->GetIndices() == std::vector<uint32>{0, 1, 2, 0, 2, 3});
		CHECK(Components[2]->Mesh->GetIndices() == std::vector<uint32>{0, 1, 2});
		CHECK(Components[2]->Mesh->GetVertices().size() == 3);
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("Parts that are not triangle lists, or are not valid, are left out with a warning")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		// Lines, then triangles whose five indices do not make whole triangles, then a good part.
		Gltf.Accessors = R"([
			{"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3", "min": [-0.5, -0.5, 0], "max": [0.5, 0.5, 0]},
			{"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
			{"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
			{"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
			{"bufferView": 3, "componentType": 5123, "count": 5, "type": "SCALAR"}])";
		Gltf.Meshes = R"([{"name": "Mixed", "primitives": [
			{"attributes": {"POSITION": 0}, "indices": 3, "mode": 1},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 4},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3}]}])";

		const WEntity* const Node =
			Test.ImportSingleNode("Mixed.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		CHECK(GetMeshComponents(*Node).size() == 1);
		REQUIRE(Test.Warnings.size() == 2);
		CHECK(Test.Warnings[0].starts_with("a part of mesh 'Mixed' is not imported: it is made of points, lines"));
		CHECK(Test.Warnings[1].starts_with("a part of mesh 'Mixed' is not imported: a mesh is made of triangles"));
	}

	TEST_CASE("A material arrives with its numbers, and glTF's own defaults where it gives none")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.More = R"("extensionsUsed": ["KHR_materials_emissive_strength"], )";
		Gltf.Meshes = R"([{"primitives": [
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 0},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 1},
			{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 2}]}])";
		Gltf.Materials = R"([
			{"name": "Full", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.4, 0.2, 1], "metallicFactor": 0.25,
			 "roughnessFactor": 0.75}, "emissiveFactor": [0.1, 0.2, 0.3]},
			{"name": "Empty"},
			{"name": "Glowing", "emissiveFactor": [1, 0.5, 0.25],
			 "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": 8}}}])";

		const WEntity* const Node =
			Test.ImportSingleNode("Materials.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		const std::vector<WStaticMeshComponent*> Components = GetMeshComponents(*Node);
		REQUIRE(Components.size() == 3);
		const WMaterial* const Full = Components[0]->Material.Get();
		const WMaterial* const Empty = Components[1]->Material.Get();
		const WMaterial* const Glowing = Components[2]->Material.Get();
		REQUIRE(Full != nullptr);
		REQUIRE(Empty != nullptr);
		REQUIRE(Glowing != nullptr);

		CHECK(Full->BaseColor == FLinearColor(0.8f, 0.4f, 0.2f, 1.0f));
		CHECK(Full->Metallic == 0.25f);
		CHECK(Full->Roughness == 0.75f);
		CHECK(Full->EmissiveColor == FLinearColor(0.1f, 0.2f, 0.3f));
		CHECK(Full->BaseColorTexture == nullptr);
		CHECK(Full->NormalScale == 1.0f);

		// A glTF material that says nothing is white, fully metallic and fully rough, and does not glow.
		CHECK(Empty->BaseColor == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		CHECK(Empty->Metallic == 1.0f);
		CHECK(Empty->Roughness == 1.0f);
		CHECK(Empty->EmissiveColor == FLinearColor(0.0f, 0.0f, 0.0f));

		CHECK(Glowing->EmissiveColor == FLinearColor(8.0f, 4.0f, 2.0f));
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("Textures are read from files beside the glTF file, as colours or as data depending on their use")
	{
		FGltfImportTest Test;
		WriteTestGltfFile(Test.TempDirectory.GetPath() / "Textures" / "My Picture.png", AsText(MakeTestPng()));
		FTestGltf Gltf;
		Gltf.Materials = TexturedMaterial;
		Gltf.More = R"("images": [{"uri": "Textures/My%20Picture.png"}],
			"samplers": [{"wrapS": 33071, "wrapT": 33648}],
			"textures": [{"source": 0, "sampler": 0}], )";

		const WEntity* const Node =
			Test.ImportSingleNode("Textured.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		const WMaterial* const Material = GetOnlyMeshComponent(*Node)->Material.Get();
		REQUIRE(Material != nullptr);
		const WTexture* const BaseColor = Material->BaseColorTexture.Get();
		const WTexture* const MetallicRoughness = Material->MetallicRoughnessTexture.Get();
		const WTexture* const Normal = Material->NormalTexture.Get();
		const WTexture* const Emissive = Material->EmissiveTexture.Get();
		REQUIRE(BaseColor != nullptr);
		REQUIRE(MetallicRoughness != nullptr);
		REQUIRE(Normal != nullptr);
		REQUIRE(Emissive != nullptr);

		REQUIRE(BaseColor->GetImage().GetWidth() == 16);
		CHECK(BaseColor->GetImage().GetPixel(0, 0) == Red);
		CHECK(BaseColor->GetImage().GetPixel(15, 0) == Green);
		CHECK(BaseColor->GetImage().GetPixel(0, 15) == Blue);
		CHECK(BaseColor->GetImage().GetPixel(15, 15) == White);

		// Colours are sRGB and data is linear. The same picture used both ways is two textures, and the same
		// picture used the same way twice is one.
		CHECK(BaseColor->GetColorSpace() == ETextureColorSpace::Srgb);
		CHECK(Emissive->GetColorSpace() == ETextureColorSpace::Srgb);
		CHECK(MetallicRoughness->GetColorSpace() == ETextureColorSpace::Linear);
		CHECK(Normal->GetColorSpace() == ETextureColorSpace::Linear);
		CHECK(BaseColor == Emissive);
		CHECK(MetallicRoughness == Normal);
		CHECK(BaseColor != Normal);

		CHECK(BaseColor->WrapU == ETextureWrap::ClampToEdge);
		CHECK(BaseColor->WrapV == ETextureWrap::MirroredRepeat);
		CHECK(Material->NormalScale == 0.5f);

		// The occlusion texture is data too, so it is the same texture as the other data textures of this picture.
		CHECK(Material->OcclusionTexture.Get() == MetallicRoughness);
		CHECK(Material->OcclusionStrength == 0.75f);
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("Textures written into the glTF file, as text or in its binary part, are read too")
	{
		const std::vector<std::byte> Png = MakeTestPng();
		FGltfImportTest Test;

		// As base64 text in the address of the image. A texture without a sampler repeats.
		FTestGltf Embedded;
		Embedded.Materials = TexturedMaterial;
		Embedded.More =
			"\"images\": [{\"uri\": \"" + MakeTestDataUri(Png, "image/png") + "\"}], \"textures\": [{\"source\": 0}], ";
		const WEntity* const FromText =
			Test.ImportSingleNode("Embedded.gltf", Embedded.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		// As a part of the binary file's buffer, after the mesh.
		FTestGltfBuffer Buffer = MakeTestSquareBuffer();
		Buffer.AppendBytes(Png);
		FTestGltf Binary;
		Binary.Materials = TexturedMaterial;
		Binary.BufferViews = R"([
			{"buffer": 0, "byteOffset": 0, "byteLength": 48},
			{"buffer": 0, "byteOffset": 48, "byteLength": 48},
			{"buffer": 0, "byteOffset": 96, "byteLength": 32},
			{"buffer": 0, "byteOffset": 128, "byteLength": 12},
			{"buffer": 0, "byteOffset": 140, "byteLength": )" +
			std::to_string(Png.size()) + "}]";
		Binary.More = R"("images": [{"bufferView": 4, "mimeType": "image/png"}], "textures": [{"source": 0}], )";
		const WEntity* const FromBinary = Test.ImportSingleNode("Binary.glb", Binary.BuildBinaryFile(Buffer));

		for (const WEntity* const Node : {FromText, FromBinary})
		{
			const WMaterial* const Material = GetOnlyMeshComponent(*Node)->Material.Get();
			REQUIRE(Material != nullptr);
			REQUIRE(Material->BaseColorTexture != nullptr);
			CHECK(Material->BaseColorTexture->GetImage().GetPixel(15, 0) == Green);
			CHECK(Material->BaseColorTexture->WrapU == ETextureWrap::Repeat);
			CHECK(Material->BaseColorTexture->WrapV == ETextureWrap::Repeat);
		}
		CHECK(Test.Warnings.empty());
	}

	TEST_CASE("A texture that cannot be read is left out with a warning, and the rest is imported")
	{
		FGltfImportTest Test;
		WriteTestGltfFile(Test.TempDirectory.GetPath() / "NotAPicture.png", "This is not a picture.");
		FTestGltf Gltf;
		Gltf.Materials = R"([{"name": "Paint", "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1],
			"baseColorTexture": {"index": 0}, "metallicRoughnessTexture": {"index": 1}},
			"normalTexture": {"index": 2, "texCoord": 1}, "emissiveTexture": {"index": 3}}])";
		Gltf.More = R"("images": [{"uri": "Missing.png"}, {"uri": "NotAPicture.png"}, {"uri": "Missing.png"},
			{"uri": "data:image/png;base64,***"}],
			"textures": [{"source": 0}, {"source": 1}, {"source": 2}, {"source": 3}], )";

		const WEntity* const Node =
			Test.ImportSingleNode("Broken.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		const WMaterial* const Material = GetOnlyMeshComponent(*Node)->Material.Get();
		REQUIRE(Material != nullptr);
		CHECK(Material->BaseColor == FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		CHECK(Material->BaseColorTexture == nullptr);
		CHECK(Material->MetallicRoughnessTexture == nullptr);
		CHECK(Material->NormalTexture == nullptr);
		CHECK(Material->EmissiveTexture == nullptr);

		REQUIRE(Test.Warnings.size() == 4);
		CHECK(Test.HasWarning("the base colour texture of material 'Paint' is left out"));
		CHECK(Test.HasWarning("Missing.png"));
		CHECK(Test.HasWarning("the metallic-roughness texture of material 'Paint' is left out"));
		CHECK(Test.HasWarning("not a PNG or JPEG image"));
		CHECK(Test.HasWarning("the normal texture of material 'Paint' uses texture coordinate set 1"));
		CHECK(Test.HasWarning(
			"the emissive texture of material 'Paint' is left out: its embedded picture is not valid base64"));
	}

	TEST_CASE("Lights arrive with glTF's colours, intensities and ranges")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Scenes = R"([{"nodes": [0, 1, 2]}])";
		Gltf.Nodes = R"([
			{"name": "Sun", "rotation": [-0.7071068, 0, 0, 0.7071068], "extensions": {"KHR_lights_punctual": {"light": 0}}},
			{"name": "Lamp", "translation": [1, 2, 3], "extensions": {"KHR_lights_punctual": {"light": 1}}},
			{"name": "Spot", "extensions": {"KHR_lights_punctual": {"light": 2}}}])";
		Gltf.More = R"("extensionsUsed": ["KHR_lights_punctual"], "extensionsRequired": ["KHR_lights_punctual"],
			"extensions": {"KHR_lights_punctual": {"lights": [
				{"type": "directional", "color": [1, 0.9, 0.8], "intensity": 100000},
				{"type": "point", "color": [0.2, 0.4, 1], "intensity": 800, "range": 12.5},
				{"type": "spot", "intensity": 5}]}}, )";

		const WEntity* const Root = Test.Import("Lights.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		INFO("Error: ", Test.Error);
		REQUIRE(Root != nullptr);
		REQUIRE(Root->GetChildren().size() == 3);

		const WEntity* const SunEntity = Root->GetChildren()[0].Get();
		const WDirectionalLightComponent* const Sun = SunEntity->FindComponent<WDirectionalLightComponent>();
		REQUIRE(Sun != nullptr);
		CHECK(Sun->Color == FLinearColor(1.0f, 0.9f, 0.8f));
		CHECK(Sun->Intensity == 100000.0f);
		// The light shines along its node's -Z axis, which this node's rotation points straight down.
		CHECK(IsNearVector(
			SunEntity->GetRelativeTransform().Rotation * FVector3f(0.0f, 0.0f, -1.0f), FVector3f(0.0f, -1.0f, 0.0f)));

		const WPointLightComponent* const Lamp = Root->GetChildren()[1]->FindComponent<WPointLightComponent>();
		REQUIRE(Lamp != nullptr);
		CHECK(Lamp->Color == FLinearColor(0.2f, 0.4f, 1.0f));
		CHECK(Lamp->Intensity == 800.0f);
		CHECK(Lamp->Range == 12.5f);

		CHECK(Root->GetChildren()[2]->GetComponents().empty());
		REQUIRE(Test.Warnings.size() == 1);
		CHECK(Test.Warnings[0] == "the light of 'Spot' is a spot light, which is not imported");
	}

	TEST_CASE("What is not imported is said in a warning, once")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Scenes = R"([{"nodes": [0, 1]}])";
		Gltf.Nodes = R"([{"name": "A", "mesh": 0, "camera": 0}, {"name": "B", "mesh": 0}])";
		Gltf.Materials = R"([{"name": "Glass", "alphaMode": "BLEND", "doubleSided": true}])";
		Gltf.More = R"("cameras": [{"type": "perspective", "perspective": {"yfov": 1, "znear": 0.1}}], )";

		const WEntity* const Root = Test.Import("Extras.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));

		INFO("Error: ", Test.Error);
		REQUIRE(Root != nullptr);
		// The material is used by two nodes and warned about once.
		REQUIRE(Test.Warnings.size() == 3);
		CHECK(Test.Warnings[0] == "the file's 1 cameras are not imported");
		CHECK(Test.Warnings[1] ==
			"material 'Glass' is transparent or cut out, which is not supported; it is imported opaque");
		CHECK(Test.Warnings[2] == "material 'Glass' is double-sided, which is not supported; only its front is drawn");

		// Warnings are logged as well, with the file they are about.
		int32 LoggedWarningCount = 0;
		for (const FLogMessage& Message : Test.Capture.GetMessages())
		{
			const bool bIsImportWarning =
				Message.Verbosity == ELogVerbosity::Warning && Message.Text.find("Extras.gltf") != std::string::npos;
			LoggedWarningCount += bIsImportWarning ? 1 : 0;
		}
		CHECK(LoggedWarningCount == 3);
	}

	TEST_CASE("The warnings are optional")
	{
		FGltfImportTest Test;
		FTestGltf Gltf;
		Gltf.Materials = R"([{"name": "Glass", "alphaMode": "BLEND"}])";
		WriteTestGltfFile(
			Test.TempDirectory.GetPath() / "Quiet.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		std::string Error;

		const WEntity* const Root =
			FGltfImporter::Import(Test.TempDirectory.GetPath() / "Quiet.gltf", *Test.Scene.Get(), Error);

		CHECK(Root != nullptr);
	}

	TEST_CASE("A file whose path has non-English letters is imported, with the files beside it")
	{
		FGltfImportTest Test;
		const std::filesystem::path Directory = Test.TempDirectory.GetPath() / std::filesystem::path(u8"Straße");
		WriteTestGltfFile(Directory / "Square.bin", AsText(MakeTestSquareBuffer().Bytes));
		WriteTestGltfFile(Directory / std::filesystem::path(u8"Grün.gltf"),
			FTestGltf().Build(R"({"byteLength": 140, "uri": "Square.bin"})"));
		std::string Error;

		const WEntity* const Root =
			FGltfImporter::Import(Directory / std::filesystem::path(u8"Grün.gltf"), *Test.Scene.Get(), Error);

		INFO("Error: ", Error);
		REQUIRE(Root != nullptr);
		REQUIRE(Root->GetChildren().size() == 1);
		CHECK(GetOnlyMeshComponent(*Root->GetChildren().front())->Mesh->GetTriangleCount() == 2);
	}

	TEST_CASE("A file that cannot be imported is refused with the reason, and the scene is left as it was")
	{
		FGltfImportTest Test;
		const FTestGltfBuffer Buffer = MakeTestSquareBuffer();

		SUBCASE("The file does not exist")
		{
			CHECK(Test.ImportFile("Missing.gltf") == nullptr);
			CHECK(Test.Error.find("Missing.gltf") != std::string::npos);
		}

		SUBCASE("The file is not glTF")
		{
			CHECK(Test.Import("Notes.gltf", "This is not a scene.") == nullptr);
			CHECK(Test.Error.find("Notes.gltf' cannot be imported: ") != std::string::npos);
		}

		SUBCASE("The file is empty")
		{
			CHECK(Test.Import("Empty.gltf", "") == nullptr);
			CHECK(Test.Error.find("Empty.gltf' cannot be imported: ") != std::string::npos);
		}

		SUBCASE("The file's text is cut off")
		{
			const std::string Content = FTestGltf().BuildWithEmbeddedBuffer(Buffer);
			CHECK(Test.Import("Cut.gltf", Content.substr(0, Content.size() / 2)) == nullptr);
			CHECK(Test.Error.find("Cut.gltf' cannot be imported: ") != std::string::npos);
		}

		SUBCASE("A binary file is cut off")
		{
			const std::string Content = FTestGltf().BuildBinaryFile(Buffer);
			CHECK(Test.Import("Cut.glb", Content.substr(0, Content.size() - 60)) == nullptr);
			CHECK(Test.Error.find("Cut.glb' cannot be imported: ") != std::string::npos);
		}

		SUBCASE("The file is glTF 1.0")
		{
			FTestGltf Gltf;
			Gltf.Version = "1.0";
			CHECK(Test.Import("Old.gltf", Gltf.BuildWithEmbeddedBuffer(Buffer)) == nullptr);
			CHECK(Test.Error.find("only glTF 2.0 is supported") != std::string::npos);
		}

		SUBCASE("The data file beside it is missing")
		{
			CHECK(
				Test.Import("NoData.gltf", FTestGltf().Build(R"({"byteLength": 140, "uri": "Gone.bin"})")) == nullptr);
			CHECK(Test.Error.find("NoData.gltf' cannot be imported: ") != std::string::npos);
			CHECK(Test.Error.find("Gone.bin") != std::string::npos);
		}

		SUBCASE("The data file beside it is shorter than the file says")
		{
			WriteTestGltfFile(Test.TempDirectory.GetPath() / "Short.bin", AsText(Buffer.Bytes).substr(0, 100));
			CHECK(Test.Import("ShortData.gltf", FTestGltf().Build(R"({"byteLength": 140, "uri": "Short.bin"})")) ==
				nullptr);
			CHECK(Test.Error.find("is 100 bytes long, but the glTF file says it holds 140 bytes") != std::string::npos);
		}

		SUBCASE("The mesh refers to data beyond the end of the buffer")
		{
			FTestGltf Gltf;
			Gltf.BufferViews = R"([
				{"buffer": 0, "byteOffset": 0, "byteLength": 48},
				{"buffer": 0, "byteOffset": 48, "byteLength": 48},
				{"buffer": 0, "byteOffset": 96, "byteLength": 32},
				{"buffer": 0, "byteOffset": 128, "byteLength": 1200}])";
			CHECK(Test.Import("Beyond.gltf", Gltf.BuildWithEmbeddedBuffer(Buffer)) == nullptr);
			CHECK(Test.Error.find("Beyond.gltf' cannot be imported: ") != std::string::npos);
		}

		SUBCASE("An index names a vertex that does not exist")
		{
			FTestGltfBuffer BadBuffer;
			BadBuffer.AppendBytes(std::span<const std::byte>(Buffer.Bytes).first(128));
			BadBuffer.Append<uint16>({0, 1, 2, 0, 2, 9});
			CHECK(Test.Import("BadIndex.gltf", FTestGltf().BuildWithEmbeddedBuffer(BadBuffer)) == nullptr);
			CHECK(Test.Error.find("BadIndex.gltf' cannot be imported: ") != std::string::npos);
		}

		SUBCASE("The file requires an extension that is not supported")
		{
			FTestGltf Gltf;
			Gltf.More =
				R"("extensionsUsed": ["KHR_draco_mesh_compression"], "extensionsRequired": ["KHR_draco_mesh_compression"], )";
			CHECK(Test.Import("Draco.gltf", Gltf.BuildWithEmbeddedBuffer(Buffer)) == nullptr);
			CHECK(Test.Error.find("it requires the extension KHR_draco_mesh_compression, which is not supported") !=
				std::string::npos);
		}

		CHECK_FALSE(Test.Error.empty());
		CHECK(Test.Scene->GetRootEntities().empty());
	}

	TEST_CASE("An imported file is drawn as it was authored")
	{
		// The textured test square from a binary file, two metres in front of the camera, with a lamp.
		const std::vector<std::byte> Png = MakeTestPng();
		FTestGltfBuffer Buffer = MakeTestSquareBuffer();
		Buffer.AppendBytes(Png);
		FTestGltf Gltf;
		Gltf.Scenes = R"([{"nodes": [0, 1]}])";
		Gltf.Nodes = R"([{"name": "Square", "mesh": 0, "translation": [0, 0, -2]},
			{"name": "Lamp", "translation": [0, 0, -1], "extensions": {"KHR_lights_punctual": {"light": 0}}}])";
		Gltf.Materials = R"([{"name": "Paint", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
			"metallicFactor": 0, "roughnessFactor": 1}}])";
		Gltf.BufferViews = R"([
			{"buffer": 0, "byteOffset": 0, "byteLength": 48},
			{"buffer": 0, "byteOffset": 48, "byteLength": 48},
			{"buffer": 0, "byteOffset": 96, "byteLength": 32},
			{"buffer": 0, "byteOffset": 128, "byteLength": 12},
			{"buffer": 0, "byteOffset": 140, "byteLength": )" +
			std::to_string(Png.size()) + "}]";
		Gltf.More = R"("extensionsUsed": ["KHR_lights_punctual"],
			"extensions": {"KHR_lights_punctual": {"lights": [{"type": "point", "intensity": 3}]}},
			"images": [{"bufferView": 4, "mimeType": "image/png"}], "samplers": [{"wrapS": 33071, "wrapT": 33071}],
			"textures": [{"source": 0, "sampler": 0}], )";

		FSceneRenderTest Test;
		const FScopedTempDirectory TempDirectory;
		WriteTestGltfFile(TempDirectory.GetPath() / "Scene.glb", Gltf.BuildBinaryFile(Buffer));
		std::string Error;
		std::vector<std::string> Warnings;
		REQUIRE(FGltfImporter::Import(TempDirectory.GetPath() / "Scene.glb", *Test.Scene.Get(), Error, &Warnings) !=
			nullptr);
		CHECK(Warnings.empty());

		// Unlit, the picture lies on the square exactly: 16 by 16 pixels of it on pixels 24 to 39.
		const FImage Unlit = Test.Render(MakeWideView());
		CHECK(Unlit.GetPixel(24, 24) == Red);
		CHECK(Unlit.GetPixel(39, 24) == Green);
		CHECK(Unlit.GetPixel(24, 39) == Blue);
		CHECK(Unlit.GetPixel(39, 39) == White);
		CHECK(Unlit.GetPixel(23, 23) == Magenta);

		// Lit by the file's lamp, each quarter shows its own colour, and the background is dark.
		Test.Renderer->ViewMode = ESceneViewMode::Lit;
		Test.Renderer->BackgroundColor = LinearBlack;
		const FImage Lit = Test.Render(MakeWideView());
		const FColor LitRed = Lit.GetPixel(28, 28);
		const FColor LitBlue = Lit.GetPixel(28, 36);
		CHECK(LitRed.R > 60);
		CHECK(LitRed.R > LitRed.G + 40);
		CHECK(LitBlue.B > 60);
		CHECK(LitBlue.B > LitBlue.R + 40);
		CHECK(Lit.GetPixel(2, 2) == Black);
		CHECK(Test.Renderer->GetStatistics().PointLightCount == 1);
		CHECK(Test.Renderer->GetStatistics().DrawnMeshCount == 1);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}
}
