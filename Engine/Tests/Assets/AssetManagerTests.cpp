#include "Assets/AssetManager.h"

#include "Assets/Asset.h"
#include "Assets/AssetPackage.h"
#include "Assets/TestAssets.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/Texture.h"
#include "Rendering/TextureColorSpace.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** Keeps an object alive for the length of a test, as a component that refers to it would. */
	class FScopedRoot
	{
	public:
		explicit FScopedRoot(WObject* InObject)
			: Object(InObject)
		{
			REQUIRE(Object != nullptr);
			Object->AddToRoot();
		}

		~FScopedRoot()
		{
			Object->RemoveFromRoot();
		}

		FScopedRoot(const FScopedRoot&) = delete;
		FScopedRoot& operator=(const FScopedRoot&) = delete;

	private:
		WObject* Object;
	};
}

TEST_SUITE("FAssetManager")
{
	TEST_CASE("A picture is loaded as a colour texture that knows its asset path")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png", FColor(10, 20, 30, 255));
		std::string Error;

		WTexture* const Texture = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);

		INFO("Error: ", Error);
		REQUIRE(Texture != nullptr);
		CHECK(Texture->GetAssetPath() == "Textures/Wood.png");
		CHECK(Texture->GetColorSpace() == ETextureColorSpace::Srgb);
		REQUIRE(Texture->HasImage());
		CHECK(Texture->GetImage().GetWidth() == 2);
		CHECK(Texture->GetImage().GetPixel(1, 1) == FColor(10, 20, 30, 255));
		REQUIRE(Texture->GetPackage() != nullptr);
		CHECK(Texture->GetPackage()->GetFile() == "Textures/Wood.png");
	}

	TEST_CASE("The key Linear loads a picture as a data texture, a different asset of the same file")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Normals.png");
		std::string Error;

		WTexture* const Linear = FAssetManager::Load<WTexture>("Textures/Normals.png#Linear", Error);
		WTexture* const Color = FAssetManager::Load<WTexture>("Textures/Normals.png", Error);

		REQUIRE(Linear != nullptr);
		REQUIRE(Color != nullptr);
		CHECK(Linear != Color);
		CHECK(Linear->GetColorSpace() == ETextureColorSpace::Linear);
		CHECK(Linear->GetAssetPath() == "Textures/Normals.png#Linear");
		CHECK(Color->GetColorSpace() == ETextureColorSpace::Srgb);
		CHECK(Linear->GetPackage() == Color->GetPackage());
		CHECK(Linear->GetPackage()->GetKeys() == std::vector<std::string>{"Linear", ""});
	}

	TEST_CASE("An HDR file is loaded as an environment map")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteEnvironment("Skies/Evening.hdr");
		std::string Error;

		const WEnvironmentMap* const Environment = FAssetManager::Load<WEnvironmentMap>("Skies/Evening.hdr", Error);

		INFO("Error: ", Error);
		REQUIRE(Environment != nullptr);
		CHECK(Environment->GetAssetPath() == "Skies/Evening.hdr");
		REQUIRE(Environment->HasImage());
		CHECK(Environment->GetImage().GetWidth() == 4);
	}

	TEST_CASE("A glTF file is loaded as its meshes and materials, each under its key")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Models/Square.gltf");
		std::string Error;

		const WAssetPackage* const Package = FAssetManager::LoadPackage("Models/Square.gltf", Error);

		INFO("Error: ", Error);
		REQUIRE(Package != nullptr);
		CHECK(Package->GetFile() == "Models/Square.gltf");
		CHECK(Package->GetKeys() == std::vector<std::string>{"Materials/0", "Meshes/0/Primitives/0"});
		CHECK(Package->GetWarnings().empty());

		const WStaticMesh* const Mesh =
			FAssetManager::Load<WStaticMesh>("Models/Square.gltf#Meshes/0/Primitives/0", Error);
		REQUIRE(Mesh != nullptr);
		CHECK(Mesh == Package->FindAsset("Meshes/0/Primitives/0"));
		CHECK(Mesh->GetAssetPath() == "Models/Square.gltf#Meshes/0/Primitives/0");
		CHECK(Mesh->GetTriangleCount() == 2);

		const WMaterial* const Material = FAssetManager::Load<WMaterial>("Models/Square.gltf#Materials/0", Error);
		REQUIRE(Material != nullptr);
		CHECK(Material->GetAssetPath() == "Models/Square.gltf#Materials/0");
		CHECK(Material->GetPackage() == Package);
	}

	TEST_CASE("The textures of a glTF file are assets of its package, once for each way they are used")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Models/Paint.png");
		FTestGltf Gltf;
		Gltf.More = R"("images": [{"uri": "Paint.png"}], "textures": [{"source": 0}], )";
		Gltf.Materials =
			R"([{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}, "normalTexture": {"index": 0}}])";
		Content.WriteText("Models/Painted.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		std::string Error;

		const WMaterial* const Material = FAssetManager::Load<WMaterial>("Models/Painted.gltf#Materials/0", Error);

		INFO("Error: ", Error);
		REQUIRE(Material != nullptr);
		REQUIRE(Material->BaseColorTexture != nullptr);
		REQUIRE(Material->NormalTexture != nullptr);
		CHECK(Material->BaseColorTexture->GetAssetPath() == "Models/Painted.gltf#Textures/0/Srgb");
		CHECK(Material->NormalTexture->GetAssetPath() == "Models/Painted.gltf#Textures/0/Linear");
		CHECK(Material->BaseColorTexture.Get() == FAssetManager::Load("Models/Painted.gltf#Textures/0/Srgb", Error));
	}

	TEST_CASE("What a glTF file leaves out is kept as the warnings of its package")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		const FScopedContentDirectory Content;
		FTestGltf Gltf;
		Gltf.Materials = R"([{"name": "Glass", "alphaMode": "BLEND"}])";
		Content.WriteText("Models/Glass.gltf", Gltf.BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		std::string Error;

		const WAssetPackage* const Package = FAssetManager::LoadPackage("Models/Glass.gltf", Error);

		REQUIRE(Package != nullptr);
		REQUIRE(Package->GetWarnings().size() == 1);
		CHECK(Package->GetWarnings()[0].find("transparent") != std::string::npos);
	}

	TEST_CASE("A file is loaded once while anything refers to one of its assets")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Square.gltf");
		std::string Error;
		WAsset* const Mesh = FAssetManager::Load("Square.gltf#Meshes/0/Primitives/0", Error);
		const FScopedRoot Root(Mesh);
		const TWeakObjectPtr<WAsset> Material = FAssetManager::Load("Square.gltf#Materials/0", Error);
		REQUIRE(Material.IsValid());

		// Nothing refers to the material itself, but the mesh keeps the whole file.
		CollectGarbage();

		REQUIRE(Material.IsValid());
		CHECK(FAssetManager::Load("Square.gltf#Meshes/0/Primitives/0", Error) == Mesh);
		CHECK(FAssetManager::Load("Square.gltf#Materials/0", Error) == Material.Get());
	}

	TEST_CASE("Assets that nothing refers to are collected, and loaded again when asked for")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Wood.png");
		std::string Error;
		const TWeakObjectPtr<WAsset> First = FAssetManager::Load("Wood.png", Error);
		REQUIRE(First.IsValid());
		const TWeakObjectPtr<WAssetPackage> FirstPackage = First.Get()->GetPackage();

		CollectGarbage();

		CHECK_FALSE(First.IsValid());
		CHECK_FALSE(FirstPackage.IsValid());
		const WAsset* const Second = FAssetManager::Load("Wood.png", Error);
		REQUIRE(Second != nullptr);
		CHECK(Second->GetAssetPath() == "Wood.png");
	}

	TEST_CASE("Load says why it cannot return an asset")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Wood.png");
		Content.WriteSquare("Square.gltf");
		Content.WriteText("Notes.txt", "Not an asset");
		Content.WriteText("Broken.png", "This is not a picture");
		Content.WriteText("Broken.gltf", "{ this is not glTF");
		std::string AssetPath;
		std::string ExpectedError;

		SUBCASE("Text that is not an asset path")
		{
			AssetPath = "../Wood.png";
			ExpectedError = "\"../Wood.png\" is not the path of an asset: it must not have the part \"..\"";
		}
		SUBCASE("A file that does not exist")
		{
			AssetPath = "Missing.png";
			ExpectedError = "the content directory has no file \"Missing.png\"";
		}
		SUBCASE("A directory")
		{
			REQUIRE(std::filesystem::create_directory(Content.GetPath() / "Folder.png"));
			AssetPath = "Folder.png";
			ExpectedError = "the content directory has no file \"Folder.png\"";
		}
		SUBCASE("A kind of file that holds no assets")
		{
			AssetPath = "Notes.txt";
			ExpectedError = "\"Notes.txt\" is not a kind of file that holds assets";
		}
		SUBCASE("A key the file does not have")
		{
			AssetPath = "Square.gltf#Meshes/7/Primitives/0";
			ExpectedError = "the file \"Square.gltf\" has no asset \"Meshes/7/Primitives/0\"";
		}
		SUBCASE("A glTF file without a key")
		{
			AssetPath = "Square.gltf";
			ExpectedError = "the file \"Square.gltf\" has no asset \"\"";
		}
		SUBCASE("A key a picture does not have")
		{
			AssetPath = "Wood.png#Bright";
			ExpectedError = "the picture \"Wood.png\" has no asset \"Bright\": a picture is a colour texture without a "
							"key, or a data texture with the key \"Linear\"";
		}

		std::string Error;
		CHECK(FAssetManager::Load(AssetPath, Error) == nullptr);
		CHECK(Error == ExpectedError);
	}

	TEST_CASE("A file that cannot be read is an error that names it")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteText("Broken.png", "This is not a picture");
		Content.WriteText("Broken.gltf", "{ this is not glTF");
		Content.WriteText("Broken.hdr", "This is not light");
		std::string Error;

		CHECK(FAssetManager::Load("Broken.png", Error) == nullptr);
		CHECK_FALSE(Error.empty());
		Error.clear();
		CHECK(FAssetManager::LoadPackage("Broken.gltf", Error) == nullptr);
		CHECK(Error.find("Broken.gltf") != std::string::npos);
		Error.clear();
		CHECK(FAssetManager::Load("Broken.hdr", Error) == nullptr);
		CHECK_FALSE(Error.empty());

		// A file that failed is not remembered as loaded: once it is repaired, it loads.
		Content.WritePicture("Broken.png");
		CHECK(FAssetManager::Load("Broken.png", Error) != nullptr);
	}

	TEST_CASE("Load of an asset of another class than the one asked for fails and says which it is")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Wood.png");
		std::string Error;

		CHECK(FAssetManager::Load<WStaticMesh>("Wood.png", Error) == nullptr);
		CHECK(Error == "the asset \"Wood.png\" is a WTexture, not a WStaticMesh");
	}

	TEST_CASE("LoadPackage wants a file, not one asset of it")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteSquare("Square.gltf");
		std::string Error;

		CHECK(FAssetManager::LoadPackage("Square.gltf#Materials/0", Error) == nullptr);
		CHECK(Error == "\"Square.gltf#Materials/0\" names one asset of a file, where the file is wanted");
	}

	TEST_CASE("Without a content directory nothing can be loaded")
	{
		const FScopedObjectTest ObjectTest;
		REQUIRE(FAssetManager::GetContentDirectory().empty());
		std::string Error;

		CHECK(FAssetManager::Load("Wood.png", Error) == nullptr);
		CHECK(Error == "the asset file \"Wood.png\" cannot be loaded: there is no content directory");
	}

	TEST_CASE("Another content directory makes the manager load from there")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		const FScopedTempDirectory Other;
		Content.WritePicture("Wood.png", FColor(255, 0, 0, 255));
		std::string Error;
		WTexture* const First = FAssetManager::Load<WTexture>("Wood.png", Error);
		const FScopedRoot Root(First);
		REQUIRE(FImageFile::SavePng(FImage(2, 2, FColor(0, 0, 255, 255)), Other.GetPath() / "Wood.png", Error));

		FAssetManager::SetContentDirectory(Other.GetPath());

		CHECK(FAssetManager::GetContentDirectory() == Other.GetPath());
		const WTexture* const Second = FAssetManager::Load<WTexture>("Wood.png", Error);
		REQUIRE(Second != nullptr);
		CHECK(Second != First);
		CHECK(Second->GetImage().GetPixel(0, 0) == FColor(0, 0, 255, 255));
		// The asset of the old directory is still what it was.
		CHECK(First->GetImage().GetPixel(0, 0) == FColor(255, 0, 0, 255));
	}

	TEST_CASE("IsAssetFile goes by the end of a file's name, in any case")
	{
		CHECK(FAssetManager::IsAssetFile("Crate.gltf"));
		CHECK(FAssetManager::IsAssetFile("Crate.GLB"));
		CHECK(FAssetManager::IsAssetFile("Folder/Wood.png"));
		CHECK(FAssetManager::IsAssetFile("Wood.Jpg"));
		CHECK(FAssetManager::IsAssetFile("Wood.jpeg"));
		CHECK(FAssetManager::IsAssetFile("Sky.hdr"));
		CHECK_FALSE(FAssetManager::IsAssetFile("Notes.txt"));
		CHECK_FALSE(FAssetManager::IsAssetFile("Level.json"));
		CHECK_FALSE(FAssetManager::IsAssetFile("png"));
		CHECK_FALSE(FAssetManager::IsAssetFile(""));
	}

	TEST_CASE("GetAssetFileOfPath gives the asset path of a file inside the content directory")
	{
		const FScopedContentDirectory Content;

		CHECK(FAssetManager::GetAssetFileOfPath(Content.GetPath() / "Models" / "Crate.glb") ==
			std::optional<std::string>("Models/Crate.glb"));
		CHECK(FAssetManager::GetAssetFileOfPath(Content.GetPath() / "Models" / ".." / "Wood.png") ==
			std::optional<std::string>("Wood.png"));
		// A file outside the directory, and the directory itself, have none.
		CHECK_FALSE(FAssetManager::GetAssetFileOfPath(Content.GetPath().parent_path() / "Wood.png").has_value());
		CHECK_FALSE(FAssetManager::GetAssetFileOfPath(Content.GetPath() / ".." / "Wood.png").has_value());
		CHECK_FALSE(FAssetManager::GetAssetFileOfPath(Content.GetPath()).has_value());
		CHECK_FALSE(FAssetManager::GetAssetFileOfPath(std::filesystem::path()).has_value());
	}

	TEST_CASE("GetAssetFileOfPath gives nothing without a content directory")
	{
		REQUIRE(FAssetManager::GetContentDirectory().empty());

		CHECK_FALSE(FAssetManager::GetAssetFileOfPath(std::filesystem::path("Wood.png")).has_value());
	}

	TEST_CASE("GetPathOfAssetFile gives the place of an asset file in the file system")
	{
		const FScopedContentDirectory Content;

		CHECK(FAssetManager::GetPathOfAssetFile("Models/Crate.glb") == Content.GetPath() / "Models" / "Crate.glb");
	}
}
