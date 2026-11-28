#include "Assets/AssetManager.h"

#include "Assets/Asset.h"
#include "Assets/AssetPackage.h"
#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
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
#include <format>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
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

	TEST_CASE("The built-in shapes are assets that need no file and no content directory")
	{
		const FScopedObjectTest ObjectTest;
		REQUIRE(FAssetManager::GetContentDirectory().empty());
		std::string Error;

		WStaticMesh* const Cube = FAssetManager::Load<WStaticMesh>(FAssetManager::CubeShape, Error);
		INFO("Error: ", Error);
		REQUIRE(Cube != nullptr);
		const FScopedRoot Root(Cube);
		const WStaticMesh* const Sphere = FAssetManager::Load<WStaticMesh>("Engine/Shapes#Sphere", Error);
		REQUIRE(Sphere != nullptr);
		const WStaticMesh* const Plane = FAssetManager::Load<WStaticMesh>(FAssetManager::PlaneShape, Error);
		REQUIRE(Plane != nullptr);

		// Each knows its name, which is what a scene file writes for it.
		CHECK(Cube->GetAssetPath() == "Engine/Shapes#Cube");
		CHECK(Sphere->GetAssetPath() == "Engine/Shapes#Sphere");
		CHECK(Plane->GetAssetPath() == "Engine/Shapes#Plane");

		// One metre each way, around the origin: the scale of an entity is then the size of what it shows.
		CHECK(Cube->GetBounds().Min == FVector3f(-0.5f));
		CHECK(Cube->GetBounds().Max == FVector3f(0.5f));
		constexpr float Tolerance = 1.0e-5f;
		CHECK(Sphere->GetBounds().Min.y == doctest::Approx(-0.5f).epsilon(Tolerance));
		CHECK(Sphere->GetBounds().Max.y == doctest::Approx(0.5f).epsilon(Tolerance));
		CHECK(Sphere->GetBounds().Max.x == doctest::Approx(0.5f).epsilon(Tolerance));
		// The plane lies flat.
		CHECK(Plane->GetBounds().Min == FVector3f(-0.5f, 0.0f, -0.5f));
		CHECK(Plane->GetBounds().Max == FVector3f(0.5f, 0.0f, 0.5f));
	}

	TEST_CASE("The built-in shapes are the assets of one file, loaded once while any of them is used")
	{
		const FScopedObjectTest ObjectTest;
		std::string Error;

		WAssetPackage* const Package = FAssetManager::LoadPackage(FAssetManager::ShapesFile, Error);
		INFO("Error: ", Error);
		REQUIRE(Package != nullptr);
		const FScopedRoot Root(Package);

		CHECK(Package->GetFile() == "Engine/Shapes");
		const std::vector<std::string> ExpectedKeys = {"Cube", "Sphere", "Plane"};
		CHECK(Package->GetKeys() == ExpectedKeys);
		CHECK(Package->GetWarnings().empty());
		CHECK(FAssetManager::Load("Engine/Shapes#Cube", Error) == Package->FindAsset("Cube"));
		CHECK(FAssetManager::LoadPackage("Engine/Shapes", Error) == Package);

		CHECK(FAssetManager::IsBuiltInFile("Engine/Shapes"));
		CHECK_FALSE(FAssetManager::IsBuiltInFile("Engine/Shapes.glb"));
		CHECK_FALSE(FAssetManager::IsBuiltInFile("Shapes"));
		CHECK_FALSE(FAssetManager::IsBuiltInFile(""));
		CHECK(FAssetManager::GetBuiltInFiles() == std::vector<std::string>{"Engine/Shapes"});
	}

	TEST_CASE("A shape that the engine does not have is an error that names it")
	{
		const FScopedObjectTest ObjectTest;
		std::string Error;

		CHECK(FAssetManager::Load("Engine/Shapes#Teapot", Error) == nullptr);
		CHECK(Error == "the file \"Engine/Shapes\" has no asset \"Teapot\"");

		// The file by itself is no shape.
		CHECK(FAssetManager::Load("Engine/Shapes", Error) == nullptr);
		CHECK(Error == "the file \"Engine/Shapes\" has no asset \"\"");

		CHECK(FAssetManager::Load<WTexture>("Engine/Shapes#Cube", Error) == nullptr);
		CHECK(Error == "the asset \"Engine/Shapes#Cube\" is a WStaticMesh, not a WTexture");
	}

	TEST_CASE("The built-in shapes stay loaded when the content directory changes")
	{
		const FScopedObjectTest ObjectTest;
		std::string Error;
		WAsset* const Cube = FAssetManager::Load(FAssetManager::CubeShape, Error);
		const FScopedRoot Root(Cube);

		{
			const FScopedContentDirectory Content;
			CHECK(FAssetManager::Load(FAssetManager::CubeShape, Error) == Cube);

			// A file of the content directory cannot take the place of the engine's.
			std::filesystem::create_directories(Content.GetPath() / "Engine");
			Content.WriteText("Engine/Shapes", "not a shape");
			CHECK(FAssetManager::Load(FAssetManager::CubeShape, Error) == Cube);
		}
		CHECK(FAssetManager::Load(FAssetManager::CubeShape, Error) == Cube);
	}

	TEST_CASE("FindLoaded returns an asset only while its file is loaded, and loads nothing")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		Content.WriteSquare("Models/Square.gltf");

		CHECK(FAssetManager::FindLoaded("Textures/Wood.png") == nullptr);
		CHECK(FAssetManager::FindLoaded("Textures/Wood.png") == nullptr);

		std::string Error;
		WTexture* const Wood = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);
		const FScopedRoot WoodRoot(Wood);
		WAssetPackage* const Square = FAssetManager::LoadPackage("Models/Square.gltf", Error);
		const FScopedRoot SquareRoot(Square);

		CHECK(FAssetManager::FindLoaded("Textures/Wood.png") == Wood);
		CHECK(FAssetManager::FindLoaded("Models/Square.gltf#Meshes/0/Primitives/0") ==
			Square->FindAsset("Meshes/0/Primitives/0"));
		// A key that the file does not have, and a picture's data texture that nothing has asked for.
		CHECK(FAssetManager::FindLoaded("Models/Square.gltf#Meshes/7/Primitives/0") == nullptr);
		CHECK(FAssetManager::FindLoaded("Textures/Wood.png#Linear") == nullptr);
		// Text that is no asset path.
		CHECK(FAssetManager::FindLoaded("../Textures/Wood.png") == nullptr);
		CHECK(FAssetManager::FindLoaded("") == nullptr);

		// The file system is not asked: an asset that is in use is found after its file is gone.
		std::filesystem::remove(Content.GetPath() / "Textures" / "Wood.png");
		CHECK(FAssetManager::FindLoaded("Textures/Wood.png") == Wood);
	}

	TEST_CASE("FindLoadedIgnoringCase finds a loaded asset under its own spelling or under other capitals")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Brick.wwmaterial");
		Content.WriteMaterial("Materials/Wall.wwmaterial");
		Content.WriteSquare("Models/Square.gltf");
		std::string Error;
		WAsset* const Brick = FAssetManager::Load("Materials/Brick.wwmaterial", Error);
		const FScopedRoot BrickRoot(Brick);
		WAsset* const Square = FAssetManager::Load("Models/Square.gltf#Materials/0", Error);
		const FScopedRoot SquareRoot(Square);
		const std::vector<WAsset*> JustBrick = {Brick};

		CHECK(FAssetManager::FindLoadedIgnoringCase("Materials/Brick.wwmaterial") == JustBrick);
		CHECK(FAssetManager::FindLoadedIgnoringCase("materials/BRICK.wwmaterial") == JustBrick);
		CHECK(FAssetManager::FindLoaded("materials/BRICK.wwmaterial") == nullptr);
		// Only the name of the file is compared without its capitals; a key is compared letter for letter.
		CHECK(FAssetManager::FindLoadedIgnoringCase("models/square.GLTF#Materials/0") == std::vector<WAsset*>{Square});
		CHECK(FAssetManager::FindLoadedIgnoringCase("Models/Square.gltf#materials/0").empty());
		// Another name, a key that the file does not have, text that is no asset path and a file that is not
		// loaded are nothing.
		CHECK(FAssetManager::FindLoadedIgnoringCase("Materials/Bricks.wwmaterial").empty());
		CHECK(FAssetManager::FindLoadedIgnoringCase("Materials/Brick.wwmaterial#Key").empty());
		CHECK(FAssetManager::FindLoadedIgnoringCase("../Brick.wwmaterial").empty());
		CHECK(FAssetManager::FindLoadedIgnoringCase("Materials/Wall.wwmaterial").empty());
	}

	TEST_CASE("FindLoadedIgnoringCase finds every spelling that is loaded, in the order of their paths")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		std::error_code ErrorCode;
		const bool bIgnoresCapitals = std::filesystem::exists(Content.GetPath() / "TEXTURES" / "WOOD.PNG", ErrorCode);
		CAPTURE(bIgnoresCapitals);
		std::string Error;
		WAsset* const Wood = FAssetManager::Load("Textures/Wood.png", Error);
		const FScopedRoot WoodRoot(Wood);
		// A picture is loaded under any name that the file system takes for its own (D-043): where capitals are not
		// told apart, this is a second package of the same file.
		WAsset* const OtherWood = FAssetManager::Load("textures/wood.png", Error);
		REQUIRE((OtherWood != nullptr) == bIgnoresCapitals);

		const std::vector<WAsset*> Found = FAssetManager::FindLoadedIgnoringCase("TEXTURES/WOOD.PNG");

		if (bIgnoresCapitals)
		{
			const FScopedRoot OtherRoot(OtherWood);
			CHECK(Found == std::vector<WAsset*>{Wood, OtherWood});
		}
		else
		{
			CHECK(Found == std::vector<WAsset*>{Wood});
		}
	}

	TEST_CASE("FindLoadedIgnoringCase finds what the engine has built in under its own name only")
	{
		const FScopedObjectTest ObjectTest;
		std::string Error;
		WAsset* const Cube = FAssetManager::Load(FAssetManager::CubeShape, Error);
		const FScopedRoot Root(Cube);

		CHECK(FAssetManager::FindLoadedIgnoringCase(FAssetManager::CubeShape) == std::vector<WAsset*>{Cube});
		CHECK(FAssetManager::FindLoadedIgnoringCase("engine/shapes#Cube").empty());
	}
	TEST_CASE("FindLoaded finds nothing of a file whose assets were collected")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		std::string Error;
		REQUIRE(FAssetManager::Load("Textures/Wood.png", Error) != nullptr);
		REQUIRE(FAssetManager::FindLoaded("Textures/Wood.png") != nullptr);

		CollectGarbage();

		CHECK(FAssetManager::FindLoaded("Textures/Wood.png") == nullptr);
	}

	TEST_CASE("FindLoaded finds a built-in shape that is in use, without a content directory")
	{
		const FScopedObjectTest ObjectTest;
		REQUIRE(FAssetManager::GetContentDirectory().empty());
		std::string Error;
		WAsset* const Cube = FAssetManager::Load(FAssetManager::CubeShape, Error);
		const FScopedRoot Root(Cube);

		CHECK(FAssetManager::FindLoaded(FAssetManager::CubeShape) == Cube);
	}

	TEST_CASE("Without a content directory nothing of one can be loaded")
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
		CHECK(FAssetManager::IsAssetFile("Materials/Brick.wwmaterial"));
		CHECK(FAssetManager::IsAssetFile("Brick.WWMATERIAL"));
		CHECK_FALSE(FAssetManager::IsAssetFile("Notes.txt"));
		CHECK_FALSE(FAssetManager::IsAssetFile("Level.wwscene"));
		CHECK_FALSE(FAssetManager::IsAssetFile("Level.json"));
		CHECK_FALSE(FAssetManager::IsAssetFile("png"));
		CHECK_FALSE(FAssetManager::IsAssetFile(""));
	}

	TEST_CASE("What a file can hold is known from its name, without the file")
	{
		const WClass& Mesh = *WStaticMesh::StaticClass();
		const WClass& Material = *WMaterial::StaticClass();
		const WClass& Texture = *WTexture::StaticClass();
		const WClass& Environment = *WEnvironmentMap::StaticClass();

		// A model has meshes, materials and textures.
		CHECK(FAssetManager::CanHoldAssetsOfClass("Models/Crate.glb", Mesh));
		CHECK(FAssetManager::CanHoldAssetsOfClass("Models/Crate.GLTF", Material));
		CHECK(FAssetManager::CanHoldAssetsOfClass("Crate.gltf", Texture));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Models/Crate.glb", Environment));

		// A picture is a texture, and an environment an environment map.
		CHECK(FAssetManager::CanHoldAssetsOfClass("Textures/Wood.png", Texture));
		CHECK(FAssetManager::CanHoldAssetsOfClass("Wood.JPG", Texture));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Textures/Wood.png", Mesh));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Textures/Wood.png", Environment));
		CHECK(FAssetManager::CanHoldAssetsOfClass("Skies/Noon.hdr", Environment));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Skies/Noon.hdr", Texture));

		// A material file is a material.
		CHECK(FAssetManager::CanHoldAssetsOfClass("Materials/Brick.wwmaterial", Material));
		CHECK(FAssetManager::CanHoldAssetsOfClass("Brick.WWMaterial", Material));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Materials/Brick.wwmaterial", Mesh));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Materials/Brick.wwmaterial", Texture));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Materials/Brick.wwmaterial", Environment));

		// The built-in shapes are meshes.
		CHECK(FAssetManager::CanHoldAssetsOfClass(FAssetManager::ShapesFile, Mesh));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass(FAssetManager::ShapesFile, Material));

		// What is of no kind that can be loaded holds nothing.
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Notes.txt", Texture));
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("", Mesh));
	}

	TEST_CASE("Whatever asks for any asset, or for any object, can be given something of every file")
	{
		for (const WClass* const Class : {WAsset::StaticClass(), WObject::StaticClass()})
		{
			CHECK(FAssetManager::CanHoldAssetsOfClass("Models/Crate.glb", *Class));
			CHECK(FAssetManager::CanHoldAssetsOfClass("Textures/Wood.png", *Class));
			CHECK(FAssetManager::CanHoldAssetsOfClass("Skies/Noon.hdr", *Class));
			CHECK(FAssetManager::CanHoldAssetsOfClass("Materials/Brick.wwmaterial", *Class));
			CHECK(FAssetManager::CanHoldAssetsOfClass(FAssetManager::ShapesFile, *Class));
			CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Notes.txt", *Class));
		}

		// What asks for something that no file has is given nothing.
		CHECK_FALSE(FAssetManager::CanHoldAssetsOfClass("Models/Crate.glb", *WAssetPackage::StaticClass()));
	}

	TEST_CASE(
		"A picture, an environment and a material file are one asset each, and a model and the shapes are several")
	{
		CHECK(FAssetManager::IsOneAsset("Textures/Wood.png"));
		CHECK(FAssetManager::IsOneAsset("Wood.JPEG"));
		CHECK(FAssetManager::IsOneAsset("Skies/Noon.hdr"));
		CHECK(FAssetManager::IsOneAsset("Materials/Brick.wwmaterial"));
		CHECK_FALSE(FAssetManager::IsOneAsset("Models/Crate.glb"));
		CHECK_FALSE(FAssetManager::IsOneAsset("Models/Crate.gltf"));
		CHECK_FALSE(FAssetManager::IsOneAsset(FAssetManager::ShapesFile));
		CHECK_FALSE(FAssetManager::IsOneAsset("Notes.txt"));
		CHECK_FALSE(FAssetManager::IsOneAsset(""));
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

	TEST_CASE("A file and its folders are spelt as the content directory spells them, as far as they are there")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Red.wwmaterial");
		Content.WritePicture("Wood.png");
		std::string Error;

		CHECK(FAssetManager::HasFileSystemSpelling("Materials/Red.wwmaterial", Error));
		CHECK(FAssetManager::HasFileSystemSpelling("Wood.png", Error));
		// What is not there yet is spelt as it is asked about, in the folders that are there.
		CHECK(FAssetManager::HasFileSystemSpelling("Materials/Blue.wwmaterial", Error));
		CHECK(FAssetManager::HasFileSystemSpelling("Materials/Metals/Gold.wwmaterial", Error));
		CHECK(FAssetManager::HasFileSystemSpelling("Other/New.wwmaterial", Error));
		// Nothing is below a file.
		CHECK(FAssetManager::HasFileSystemSpelling("Wood.png/New.wwmaterial", Error));
		CHECK(Error.empty());
	}

	TEST_CASE("Other capitals of a name are not its spelling where the file system takes them for the name")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Red.wwmaterial");
		// Whether the file system takes one spelling for another is asked of the directory, so that the test holds
		// on every file system.
		std::error_code ErrorCode;
		const bool bIgnoresCapitals =
			std::filesystem::exists(Content.GetPath() / "MATERIALS" / "RED.WWMATERIAL", ErrorCode);
		CAPTURE(bIgnoresCapitals);

		struct FSpelling
		{
			std::string File;
			std::string Kind;
			std::string Name;
		};
		const std::vector<FSpelling> Spellings = {
			{"materials/Red.wwmaterial", "folder", "materials"},
			{"Materials/red.wwmaterial", "file", "Materials/red.wwmaterial"},
			{"MATERIALS/RED.WWMATERIAL", "folder", "MATERIALS"},
			// A file that is not there yet, in a folder that is.
			{"materials/New.wwmaterial", "folder", "materials"},
		};
		for (const FSpelling& Spelling : Spellings)
		{
			CAPTURE(Spelling.File);
			std::string Error;
			if (bIgnoresCapitals)
			{
				CHECK_FALSE(FAssetManager::HasFileSystemSpelling(Spelling.File, Error));
				CHECK(Error ==
					std::format("the content directory has no {} \"{}\", only one whose name is spelt differently",
						Spelling.Kind,
						Spelling.Name));
			}
			else
			{
				// Another name is of another file or folder, which is not there.
				CHECK(FAssetManager::HasFileSystemSpelling(Spelling.File, Error));
			}
		}
	}

	TEST_CASE("A name that ends in a period or a space is the spelling of nothing, on every file system")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Red.wwmaterial");

		struct FName
		{
			std::string File;
			std::string Ending;
			std::string Part;
		};
		const std::vector<FName> Names = {
			{"Materials./Red.wwmaterial", "period", "Materials."},
			{"Materials /Red.wwmaterial", "space", "Materials "},
			{"Materials/Red.wwmaterial.", "period", "Red.wwmaterial."},
			// A folder that is not there yet: Windows would make it under the name without the period.
			{"New./Brick.wwmaterial", "period", "New."},
		};
		for (const FName& Name : Names)
		{
			CAPTURE(Name.File);
			std::string Error;
			CHECK_FALSE(FAssetManager::HasFileSystemSpelling(Name.File, Error));
			CHECK(Error ==
				std::format("the name \"{}\" ends in a {}, which Windows does not keep", Name.Part, Name.Ending));
		}
	}

	TEST_CASE("A part that is there under other capitals is called a file or a folder as it is on disk")
	{
		const FScopedContentDirectory Content;
		Content.WritePicture("Wood.png");
		std::error_code ErrorCode;
		const bool bIgnoresCapitals = std::filesystem::exists(Content.GetPath() / "WOOD.PNG", ErrorCode);
		CAPTURE(bIgnoresCapitals);
		std::string Error;

		// Meant as a folder, and a file on disk.
		const bool bHasSpelling = FAssetManager::HasFileSystemSpelling("wood.png/Brick.wwmaterial", Error);

		if (bIgnoresCapitals)
		{
			CHECK_FALSE(bHasSpelling);
			CHECK(Error == "the content directory has no file \"wood.png\", only one whose name is spelt differently");
		}
		else
		{
			CHECK(bHasSpelling);
		}
	}

	TEST_CASE("The spelling of a file cannot be told without a content directory, nor of text that names no file")
	{
		std::string Error;
		REQUIRE(FAssetManager::GetContentDirectory().empty());
		CHECK_FALSE(FAssetManager::HasFileSystemSpelling("Materials/Red.wwmaterial", Error));
		CHECK(Error == "there is no content directory");

		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Red.wwmaterial");
		CHECK_FALSE(FAssetManager::HasFileSystemSpelling("../Red.wwmaterial", Error));
		CHECK(Error == "\"../Red.wwmaterial\" is not the path of an asset: it must not have the part \"..\"");
		CHECK_FALSE(FAssetManager::HasFileSystemSpelling("", Error));
		CHECK(Error == "\"\" is not the path of an asset: it names no file");
		CHECK_FALSE(FAssetManager::HasFileSystemSpelling("Materials/Red.wwmaterial#Key", Error));
		CHECK(Error == "\"Materials/Red.wwmaterial#Key\" names one asset of a file, where the file is wanted");
	}

	TEST_CASE("The spelling of a file cannot be told where a folder cannot be listed, and the reason says which")
	{
		const FScopedContentDirectory Content;
		// A content directory that is not there is a folder that cannot be listed.
		const std::filesystem::path Missing = Content.GetPath() / "Missing";
		FAssetManager::SetContentDirectory(Missing);
		std::string Error;

		CHECK_FALSE(FAssetManager::HasFileSystemSpelling("Materials/Red.wwmaterial", Error));

		const std::string Expected =
			std::format("the folder \"{}\" cannot be listed: ", FFileHelper::PathToUtf8(Missing));
		CHECK(Error.starts_with(Expected));
		CHECK(Error.size() > Expected.size());
	}

	TEST_CASE("A picture and a model are loaded under any name that the file system takes for theirs")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		Content.WriteSquare("Models/Square.gltf");
		std::error_code ErrorCode;
		const bool bIgnoresCapitals = std::filesystem::exists(Content.GetPath() / "TEXTURES" / "WOOD.PNG", ErrorCode);
		CAPTURE(bIgnoresCapitals);
		std::string Error;

		// Only material files are held to the spelling of their directory (see AssetManagerMaterialTests.cpp).
		CHECK((FAssetManager::Load("textures/wood.png", Error) != nullptr) == bIgnoresCapitals);
		CHECK((FAssetManager::LoadPackage("models/square.gltf", Error) != nullptr) == bIgnoresCapitals);
	}
}
