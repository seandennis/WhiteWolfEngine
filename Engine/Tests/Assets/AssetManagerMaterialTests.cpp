#include "Assets/AssetManager.h"

#include "Assets/Asset.h"
#include "Assets/AssetPackage.h"
#include "Assets/TestAssets.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/Material.h"
#include "Rendering/Texture.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

// Material files, as the asset manager loads them. What a material file can say, and how it is read, is tested
// with FMaterialSerializer; these tests are about the file being an asset of the content directory.

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

	/** Loads a material that the test expects to load. */
	WMaterial* LoadMaterial(const std::string& AssetPath)
	{
		std::string Error;
		WMaterial* const Material = FAssetManager::Load<WMaterial>(AssetPath, Error);
		INFO("Error: ", Error);
		REQUIRE(Material != nullptr);
		return Material;
	}

	/** Returns the warnings that have been logged, in their order. */
	std::vector<std::string> GetLoggedWarnings(const FScopedLogCapture& Capture)
	{
		std::vector<std::string> Warnings;
		for (const FLogMessage& Message : Capture.GetMessages())
		{
			if (Message.Verbosity == ELogVerbosity::Warning)
			{
				Warnings.push_back(Message.Text);
			}
		}
		return Warnings;
	}
}

TEST_SUITE("FAssetManager material files")
{
	TEST_CASE("A material file is one material, whose asset path is the file's name")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		const FScopedLogCapture Capture;
		Content.WritePicture("Textures/Wood.png");
		Content.WriteMaterial("Materials/Red.wwmaterial",
			R"({"BaseColor": [1, 0, 0, 1], "Roughness": 0.25, "BaseColorTexture": "Textures/Wood.png"})");

		WMaterial* const Material = LoadMaterial("Materials/Red.wwmaterial");
		const FScopedRoot Root(Material);

		CHECK(Material->GetAssetPath() == "Materials/Red.wwmaterial");
		CHECK(Material->BaseColor == FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		CHECK(Material->Roughness == 0.25f);
		// What the file does not say is what a new material has.
		CHECK(Material->Metallic == 0.0f);
		REQUIRE(Material->GetPackage() != nullptr);
		CHECK(Material->GetPackage()->FindAsset("") == Material);
		CHECK(Material->GetPackage()->GetAssets().size() == 1);
		CHECK(Material->GetPackage()->GetWarnings().empty());

		// Its texture is the asset of the picture, as anything else that asks for it gets it.
		std::string Error;
		CHECK(Material->BaseColorTexture.Get() == FAssetManager::Load<WTexture>("Textures/Wood.png", Error));
		CHECK(CountLoggedProblems(Capture) == 0);
	}

	TEST_CASE("A material file is loaded once while it is used, and read anew when it is asked for after that")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		Content.WriteMaterial(
			"Materials/Red.wwmaterial", R"({"Metallic": 0.5, "BaseColorTexture": "Textures/Wood.png"})");

		TWeakObjectPtr<WMaterial> First;
		TWeakObjectPtr<WTexture> Texture;
		{
			WMaterial* const Material = LoadMaterial("Materials/Red.wwmaterial");
			const FScopedRoot Root(Material);
			First = Material;
			Texture = Material->BaseColorTexture.Get();
			CHECK(LoadMaterial("Materials/Red.wwmaterial") == Material);

			// The texture lives as long as the material does: the material keeps it.
			CollectGarbage();
			CHECK(Texture.IsValid());
		}

		CollectGarbage();
		CHECK_FALSE(First.IsValid());
		CHECK_FALSE(Texture.IsValid());

		// The file has changed meanwhile, and the material that is read now is what it says now.
		Content.WriteMaterial("Materials/Red.wwmaterial", R"({"Metallic": 1.0})");
		WMaterial* const Again = LoadMaterial("Materials/Red.wwmaterial");
		CHECK(Again->Metallic == 1.0f);
		CHECK(Again->BaseColorTexture == nullptr);
	}

	TEST_CASE("What a material file could not give its material is the warnings of its package, logged once each")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		const FScopedLogCapture Capture;
		Content.WriteMaterial("Materials/Odd.wwmaterial",
			R"({"Colour": [1, 0, 0, 1], "BaseColorTexture": "Textures/Gone.png", "Metallic": 1})");

		WMaterial* const Material = LoadMaterial("Materials/Odd.wwmaterial");

		const std::vector<std::string> Expected = {
			R"(there is no property named "Colour"; the value was ignored)",
			R"(property "BaseColorTexture": the content directory has no file "Textures/Gone.png")",
		};
		CHECK(Material->GetPackage()->GetWarnings() == Expected);
		CHECK(Material->Metallic == 1.0f);
		// The log says which file each is about.
		std::vector<std::string> Logged;
		for (const std::string& Warning : Expected)
		{
			Logged.push_back("Loading 'Materials/Odd.wwmaterial': " + Warning);
		}
		CHECK(GetLoggedWarnings(Capture) == Logged);
	}

	TEST_CASE("A material file that cannot be read is an error, and is read once it is repaired")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		std::string Text;
		std::string Expected;

		SUBCASE("Text that is not a material")
		{
			Text = "[]";
			Expected = "a material must be a JSON object, not an array";
		}
		SUBCASE("A newer version")
		{
			Text = R"({"Format": "WhiteWolfMaterial", "Version": 7, "Properties": {}})";
			Expected = "the material was saved in format version 7, but this build reads only up to version 1";
		}
		Content.WriteText("Materials/Broken.wwmaterial", Text);

		std::string Error;
		CHECK(FAssetManager::Load("Materials/Broken.wwmaterial", Error) == nullptr);
		CHECK(Error.ends_with(Expected));
		CHECK(Error.find("Broken.wwmaterial: ") != std::string::npos);

		// Nothing was kept of it: once the file is right, it is read.
		Content.WriteMaterial("Materials/Broken.wwmaterial", R"({"Roughness": 0.0})");
		CHECK(LoadMaterial("Materials/Broken.wwmaterial")->Roughness == 0.0f);
	}

	TEST_CASE("A material file that is not there, a key on one, and a texture asked for of one are refused")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Red.wwmaterial");
		std::string Error;

		CHECK(FAssetManager::Load("Materials/Gone.wwmaterial", Error) == nullptr);
		CHECK(Error == "the content directory has no file \"Materials/Gone.wwmaterial\"");

		CHECK(FAssetManager::Load("Materials/Red.wwmaterial#Materials/0", Error) == nullptr);
		CHECK(Error == "the file \"Materials/Red.wwmaterial\" has no asset \"Materials/0\"");

		CHECK(FAssetManager::Load<WTexture>("Materials/Red.wwmaterial", Error) == nullptr);
		CHECK(Error == "the asset \"Materials/Red.wwmaterial\" is a WMaterial, not a WTexture");
	}

	TEST_CASE("A material file that refers to itself is read without that reference, which is a warning")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		const FScopedLogCapture Capture;
		Content.WriteMaterial(
			"Materials/Loop.wwmaterial", R"({"BaseColorTexture": "Materials/Loop.wwmaterial", "Metallic": 1.0})");

		WMaterial* const Material = LoadMaterial("Materials/Loop.wwmaterial");

		CHECK(Material->BaseColorTexture == nullptr);
		CHECK(Material->Metallic == 1.0f);
		const std::vector<std::string> Expected = {R"(property "BaseColorTexture": the asset file )"
												   R"("Materials/Loop.wwmaterial" refers to itself, directly or )"
												   R"(through another file)"};
		CHECK(Material->GetPackage()->GetWarnings() == Expected);
		CHECK(CountLoggedProblems(Capture) == 1);
	}

	TEST_CASE("Two material files that refer to each other are both read, in either order")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/A.wwmaterial", R"({"BaseColorTexture": "Materials/B.wwmaterial"})");
		Content.WriteMaterial("Materials/B.wwmaterial", R"({"NormalTexture": "Materials/A.wwmaterial"})");
		Content.WriteMaterial("Materials/C.wwmaterial");
		std::string First;
		std::string Second;

		SUBCASE("A first")
		{
			First = "Materials/A.wwmaterial";
			Second = "Materials/B.wwmaterial";
		}
		SUBCASE("B first")
		{
			First = "Materials/B.wwmaterial";
			Second = "Materials/A.wwmaterial";
		}

		// The one that is asked for first reads the other, which finds the first still being read. The other is
		// a material, which is no texture: each has a warning, and neither has a texture.
		WMaterial* const FirstMaterial = LoadMaterial(First);
		const FScopedRoot Root(FirstMaterial);
		WMaterial* const SecondMaterial = LoadMaterial(Second);
		CHECK(FirstMaterial->GetPackage()->GetWarnings().size() == 1);
		CHECK(SecondMaterial->GetPackage()->GetWarnings().size() == 1);
		CHECK(FirstMaterial->BaseColorTexture == nullptr);
		CHECK(FirstMaterial->NormalTexture == nullptr);
		CHECK(SecondMaterial->BaseColorTexture == nullptr);
		CHECK(SecondMaterial->NormalTexture == nullptr);

		// Nothing is left being read: another file, and the same ones once they are collected, are read as ever.
		CHECK(LoadMaterial("Materials/C.wwmaterial")->GetPackage()->GetWarnings().empty());
	}

	TEST_CASE("After a material file that could not be read, files are read as ever")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteText("Materials/Broken.wwmaterial", "[]");
		Content.WriteMaterial("Materials/Red.wwmaterial", R"({"BaseColorTexture": "Materials/Broken.wwmaterial"})");
		std::string Error;

		CHECK(FAssetManager::Load("Materials/Broken.wwmaterial", Error) == nullptr);
		WMaterial* const Red = LoadMaterial("Materials/Red.wwmaterial");
		REQUIRE(Red->GetPackage()->GetWarnings().size() == 1);
		CHECK(Red->GetPackage()->GetWarnings()[0].find("a material must be a JSON object, not an array") !=
			std::string::npos);
		CHECK(FAssetManager::Load("Materials/Broken.wwmaterial", Error) == nullptr);
		CHECK(Error.ends_with("a material must be a JSON object, not an array"));
	}
}
