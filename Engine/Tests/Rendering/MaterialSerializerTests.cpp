#include "Rendering/MaterialSerializer.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Json/JsonValue.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Rendering/Material.h"
#include "Rendering/Texture.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** The text of the file of a new material, to the byte. */
	const std::string NewMaterialText = R"({
  "Format": "WhiteWolfMaterial",
  "Version": 1,
  "Properties": {
    "BaseColor": [1.0, 1.0, 1.0, 1.0],
    "Metallic": 0.0,
    "Roughness": 0.5,
    "EmissiveColor": [0.0, 0.0, 0.0, 1.0],
    "BaseColorTexture": null,
    "MetallicRoughnessTexture": null,
    "NormalTexture": null,
    "NormalScale": 1.0,
    "EmissiveTexture": null,
    "OcclusionTexture": null,
    "OcclusionStrength": 1.0
  }
}
)";

	/** Returns the text of a material document whose "Properties" is the given JSON text. */
	std::string MakeDocument(const std::string& Properties)
	{
		return R"({"Format": "WhiteWolfMaterial", "Version": 1, "Properties": )" + Properties + "}";
	}

	/** Loads text that the test expects to be taken into a material, and returns the warnings. */
	std::vector<std::string> Load(std::string_view Text, WMaterial& Material)
	{
		std::string Error;
		std::vector<std::string> Warnings;
		const bool bLoaded = FMaterialSerializer::LoadFromText(Text, Material, Error, Warnings);
		INFO("Error: ", Error);
		REQUIRE(bLoaded);
		return Warnings;
	}

	/** Returns how many messages of a verbosity have been logged. */
	int32 CountMessages(const FScopedLogCapture& Capture, ELogVerbosity Verbosity)
	{
		int32 Count = 0;
		for (const FLogMessage& Message : Capture.GetMessages())
		{
			Count += Message.Verbosity == Verbosity ? 1 : 0;
		}
		return Count;
	}
}

TEST_SUITE("FMaterialSerializer")
{
	TEST_CASE("A new material is written with every property, in the order of the class, its defaults included")
	{
		const FScopedObjectTest ObjectTest;

		CHECK(FMaterialSerializer::SaveToText(*NewObject<WMaterial>()) == NewMaterialText);
	}

	TEST_CASE("Numbers, colours and textures are written as numbers, four numbers and asset paths")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		std::string Error;
		WMaterial* const Material = NewObject<WMaterial>();
		Material->BaseColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		Material->Metallic = 1.0f;
		Material->Roughness = 0.3f;
		Material->BaseColorTexture = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);
		Material->NormalTexture = FAssetManager::Load<WTexture>("Textures/Wood.png#Linear", Error);
		REQUIRE(Material->BaseColorTexture != nullptr);
		REQUIRE(Material->NormalTexture != nullptr);

		const FJsonValue Document = FMaterialSerializer::SaveToJson(*Material);

		const FJsonValue& Properties = *Document.FindMember("Properties");
		CHECK(*Properties.FindMember("BaseColor") ==
			FJsonValue(FJsonValue::FArray{FJsonValue(1.0f), FJsonValue(0.0f), FJsonValue(0.0f), FJsonValue(1.0f)}));
		CHECK(*Properties.FindMember("Metallic") == FJsonValue(1.0f));
		CHECK(*Properties.FindMember("BaseColorTexture") == FJsonValue("Textures/Wood.png"));
		CHECK(*Properties.FindMember("NormalTexture") == FJsonValue("Textures/Wood.png#Linear"));
		CHECK(Properties.FindMember("MetallicRoughnessTexture")->IsNull());
		// A float is written with the fewest digits that read back as the same float.
		CHECK(FMaterialSerializer::SaveToText(*Material).find("\"Roughness\": 0.3,") != std::string::npos);
	}

	TEST_CASE("A texture that has no asset path is written as null, with one warning that names the property")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WMaterial* const Material = NewObject<WMaterial>();
		Material->BaseColorTexture = NewObject<WTexture>();

		const FJsonValue Document = FMaterialSerializer::SaveToJson(*Material);

		CHECK(Document.FindMember("Properties")->FindMember("BaseColorTexture")->IsNull());
		REQUIRE(CountMessages(Capture, ELogVerbosity::Warning) == 1);
		CHECK(Capture.GetMessages().back().Text ==
			"Saving a material: property BaseColorTexture refers to a WTexture that has no asset path; the reference "
			"is saved as null");

		// A texture that was destroyed is no texture: it is written as null, and nothing is said about it.
		WTexture* const Destroyed = NewObject<WTexture>();
		Material->BaseColorTexture = Destroyed;
		Destroyed->Destroy();
		CHECK(FMaterialSerializer::SaveToJson(*Material)
				  .FindMember("Properties")
				  ->FindMember("BaseColorTexture")
				  ->IsNull());
		CHECK(CountMessages(Capture, ELogVerbosity::Warning) == 1);
	}

	TEST_CASE("A material is written to a file in folders that are made, and replaces the file that is there")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Materials" / "Metals" / "Red.wwmaterial";
		WMaterial* const Material = NewObject<WMaterial>();
		std::string Error;

		REQUIRE(FMaterialSerializer::SaveToFile(*Material, Path, Error));
		std::string Text;
		REQUIRE(FFileHelper::LoadFileToString(Path, Text, Error));
		CHECK(Text == NewMaterialText);

		Material->Metallic = 1.0f;
		REQUIRE(FMaterialSerializer::SaveToFile(*Material, Path, Error));
		REQUIRE(FFileHelper::LoadFileToString(Path, Text, Error));
		CHECK(Text == FMaterialSerializer::SaveToText(*Material));
		CHECK(Text != NewMaterialText);
	}

	TEST_CASE("A file that cannot be written says why, and leaves what is there")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedTempDirectory Directory;
		// Where the file should be is a folder, with something in it.
		const std::filesystem::path Path = Directory.GetPath() / "Red.wwmaterial";
		std::filesystem::create_directories(Path);
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile("Kept", Path / "Inside.txt", Error));

		CHECK_FALSE(FMaterialSerializer::SaveToFile(*NewObject<WMaterial>(), Path, Error));

		CHECK_FALSE(Error.empty());
		CHECK(std::filesystem::is_directory(Path));
		std::string Kept;
		REQUIRE(FFileHelper::LoadFileToString(Path / "Inside.txt", Kept, Error));
		CHECK(Kept == "Kept");
	}

	TEST_CASE("A material file is known by the end of its name, in any case of letters")
	{
		CHECK(FMaterialSerializer::IsMaterialFile("Brick.wwmaterial"));
		CHECK(FMaterialSerializer::IsMaterialFile("Materials/Brick.WWMaterial"));
		CHECK_FALSE(FMaterialSerializer::IsMaterialFile("Brick.json"));
		CHECK_FALSE(FMaterialSerializer::IsMaterialFile("Brick.wwscene"));
		CHECK_FALSE(FMaterialSerializer::IsMaterialFile("Brick"));
		CHECK_FALSE(FMaterialSerializer::IsMaterialFile("Brick.wwmaterial.txt"));
	}

	TEST_CASE("A material that is saved after it was loaded is the text it was loaded from")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		std::string Error;
		WMaterial* const Written = NewObject<WMaterial>();
		Written->BaseColor = FLinearColor(0.25f, 0.5f, 1.0f, 1.0f);
		Written->Roughness = 0.125f;
		Written->EmissiveColor = FLinearColor(4.0f, 0.0f, 0.0f, 1.0f);
		Written->BaseColorTexture = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);
		Written->OcclusionTexture = FAssetManager::Load<WTexture>("Textures/Wood.png#Linear", Error);
		const std::string Text = FMaterialSerializer::SaveToText(*Written);

		WMaterial* const Read = NewObject<WMaterial>();
		CHECK(Load(Text, *Read).empty());

		CHECK(FMaterialSerializer::SaveToText(*Read) == Text);
	}

	TEST_CASE("Loading a material loads its textures, a picture as a colour and as data being two assets of one file")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		WMaterial* const Material = NewObject<WMaterial>();

		CHECK(Load(MakeDocument(R"({"BaseColorTexture": "Textures/Wood.png",
			"NormalTexture": "Textures/Wood.png#Linear"})"),
			*Material)
				  .empty());

		std::string Error;
		const WTexture* const Colour = FAssetManager::Load<WTexture>("Textures/Wood.png", Error);
		const WTexture* const Data = FAssetManager::Load<WTexture>("Textures/Wood.png#Linear", Error);
		CHECK(Material->BaseColorTexture.Get() == Colour);
		CHECK(Material->NormalTexture.Get() == Data);
		CHECK(Colour != Data);
		REQUIRE(Colour != nullptr);
		REQUIRE(Data != nullptr);
		CHECK(Colour->GetPackage() == Data->GetPackage());
	}

	TEST_CASE("A whole number is read where a number with a point is meant, and written back with the point")
	{
		const FScopedObjectTest ObjectTest;
		WMaterial* const Material = NewObject<WMaterial>();

		CHECK(Load(MakeDocument(R"({"Metallic": 1, "BaseColor": [1, 0, 0, 1]})"), *Material).empty());

		CHECK(Material->Metallic == 1.0f);
		CHECK(Material->BaseColor == FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		const std::string Text = FMaterialSerializer::SaveToText(*Material);
		CHECK(Text.find("\"Metallic\": 1.0,") != std::string::npos);
		CHECK(Text.find("\"BaseColor\": [1.0, 0.0, 0.0, 1.0],") != std::string::npos);
	}

	TEST_CASE("What a document does not name keeps its value, and a document without properties changes nothing")
	{
		const FScopedObjectTest ObjectTest;
		WMaterial* const Material = NewObject<WMaterial>();
		Material->Roughness = 0.9f;

		CHECK(Load(MakeDocument(R"({"Metallic": 1.0})"), *Material).empty());
		CHECK(Material->Metallic == 1.0f);
		CHECK(Material->Roughness == 0.9f);

		const std::string Before = FMaterialSerializer::SaveToText(*Material);
		CHECK(Load(R"({"Format": "WhiteWolfMaterial", "Version": 1})", *Material).empty());
		CHECK(FMaterialSerializer::SaveToText(*Material) == Before);
	}

	TEST_CASE("What was wrong with a document is returned, not logged, and its textures are loaded as any file is")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		const FScopedLogCapture Capture;
		WMaterial* const Material = NewObject<WMaterial>();

		const std::string Properties = R"({"Colour": 1, "BaseColorTexture": "Textures/Wood.png", )"
									   R"("NormalTexture": "Textures/Gone.png"})";

		const std::vector<std::string> Warnings = Load(MakeDocument(Properties), *Material);

		CHECK(Warnings.size() == 2);
		// The one line is the asset manager's, of the picture that the document names and that was loaded.
		REQUIRE(Capture.GetMessages().size() == 1);
		CHECK(Capture.GetMessages()[0].Verbosity == ELogVerbosity::Log);
		CHECK(Capture.GetMessages()[0].Text == "Loaded 'Textures/Wood.png': 0 assets, 0 warnings");
	}

	TEST_CASE("The warning about a texture that cannot be written names a material that has an asset path by it")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		Content.WriteMaterial("Materials/Red.wwmaterial");
		std::string Error;
		WMaterial* const Material = FAssetManager::Load<WMaterial>("Materials/Red.wwmaterial", Error);
		REQUIRE(Material != nullptr);
		Material->BaseColorTexture = NewObject<WTexture>();
		const FScopedLogCapture Capture;

		const FJsonValue Document = FMaterialSerializer::SaveToJson(*Material);

		CHECK(Document.FindMember("Properties")->FindMember("BaseColorTexture")->IsNull());
		REQUIRE(CountMessages(Capture, ELogVerbosity::Warning) == 1);
		CHECK(Capture.GetMessages().back().Text ==
			"Saving the material 'Materials/Red.wwmaterial': property BaseColorTexture refers to a WTexture that has "
			"no "
			"asset path; the reference is saved as null");
	}

	TEST_CASE("A document that is not a material of this build is refused, and the material is left as it was")
	{
		const FScopedObjectTest ObjectTest;
		WMaterial* const Material = NewObject<WMaterial>();
		Material->Roughness = 0.75f;
		const std::string Before = FMaterialSerializer::SaveToText(*Material);
		std::string Text;
		std::string Expected;

		SUBCASE("An array")
		{
			Text = "[1, 2]";
			Expected = "a material must be a JSON object, not an array";
		}
		SUBCASE("No format")
		{
			Text = R"({"Version": 1, "Properties": {"Roughness": 0.1}})";
			Expected = R"(this is not a material: "Format" must be "WhiteWolfMaterial")";
		}
		SUBCASE("Another format")
		{
			Text = R"({"Format": "WhiteWolfScene", "Version": 1, "Properties": {"Roughness": 0.1}})";
			Expected = R"(this is not a material: "Format" must be "WhiteWolfMaterial")";
		}
		SUBCASE("No version")
		{
			Text = R"({"Format": "WhiteWolfMaterial", "Properties": {"Roughness": 0.1}})";
			Expected = R"("Version" must be a positive whole number)";
		}
		SUBCASE("A version that is a string")
		{
			Text = R"({"Format": "WhiteWolfMaterial", "Version": "1", "Properties": {"Roughness": 0.1}})";
			Expected = R"("Version" must be a positive whole number)";
		}
		SUBCASE("A version of nought")
		{
			Text = R"({"Format": "WhiteWolfMaterial", "Version": 0, "Properties": {"Roughness": 0.1}})";
			Expected = R"("Version" must be a positive whole number)";
		}
		SUBCASE("A newer version")
		{
			Text = R"({"Format": "WhiteWolfMaterial", "Version": 2, "Properties": {"Roughness": 0.1}})";
			Expected = "the material was saved in format version 2, but this build reads only up to version 1";
		}
		SUBCASE("Properties that are an array")
		{
			Text = R"({"Format": "WhiteWolfMaterial", "Version": 1, "Properties": [0.1]})";
			Expected = R"("Properties" of the material must be an object, not an array)";
		}

		std::string Error;
		std::vector<std::string> Warnings;
		CHECK_FALSE(FMaterialSerializer::LoadFromText(Text, *Material, Error, Warnings));

		CHECK(Error == Expected);
		CHECK(Warnings.empty());
		CHECK(FMaterialSerializer::SaveToText(*Material) == Before);
	}

	TEST_CASE("Text that is not JSON is refused with where it went wrong")
	{
		const FScopedObjectTest ObjectTest;
		WMaterial* const Material = NewObject<WMaterial>();
		std::string Error;
		std::vector<std::string> Warnings;

		CHECK_FALSE(FMaterialSerializer::LoadFromText("{ not a material", *Material, Error, Warnings));

		CHECK(Error.starts_with("the material is not valid JSON: line 1, column "));
		CHECK(FMaterialSerializer::SaveToText(*Material) == NewMaterialText);
	}

	TEST_CASE("A file that is not there, or not a material, is refused with its path")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedTempDirectory Directory;
		WMaterial* const Material = NewObject<WMaterial>();
		std::string Error;
		std::vector<std::string> Warnings;

		const std::filesystem::path Missing = Directory.GetPath() / "Missing.wwmaterial";
		CHECK_FALSE(FMaterialSerializer::LoadFromFile(Missing, *Material, Error, Warnings));
		std::string Expected;
		std::string Unused;
		CHECK_FALSE(FFileHelper::LoadFileToString(Missing, Unused, Expected));
		CHECK(Error == Expected);

		const std::filesystem::path Wrong = Directory.GetPath() / "Wrong.wwmaterial";
		REQUIRE(FFileHelper::SaveStringToFile("[]", Wrong, Error));
		CHECK_FALSE(FMaterialSerializer::LoadFromFile(Wrong, *Material, Error, Warnings));
		CHECK(Error == FFileHelper::PathToUtf8(Wrong) + ": a material must be a JSON object, not an array");
		CHECK(FMaterialSerializer::SaveToText(*Material) == NewMaterialText);
	}

	TEST_CASE("A value that cannot be used is a warning, and the other values are read")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedContentDirectory Content;
		WMaterial* const Material = NewObject<WMaterial>();

		const std::vector<std::string> Warnings = Load(
			R"({"Format": "WhiteWolfMaterial", "Version": 1, "Propertys": {}, "Properties": {
				"Colour": [1, 0, 0, 1],
				"Package": null,
				"Roughness": "smooth",
				"Metallic": 1.0,
				"BaseColor": [1, 0, 0],
				"BaseColorTexture": "../Wood.png",
				"NormalTexture": "Textures/Gone.png",
				"OcclusionTexture": "Engine/Shapes#Cube",
				"EmissiveTexture": 3,
				"NormalScale": 0.5
			}})",
			*Material);

		const std::vector<std::string> Expected = {
			R"(the material has the unknown key "Propertys", which was ignored)",
			R"(there is no property named "Colour"; the value was ignored)",
			R"(the property "Package" is not saved with a material; the value was ignored)",
			R"(property "Roughness": expected a number but found a string)",
			R"(property "BaseColor": expected an array of 4 numbers for FLinearColor)",
			R"(property "BaseColorTexture": "../Wood.png" is not the path of an asset: it must not have the part "..")",
			R"(property "NormalTexture": the content directory has no file "Textures/Gone.png")",
			R"(property "OcclusionTexture": the reference is to a WStaticMesh, which is not a WTexture)",
			R"(property "EmissiveTexture": expected the path of an asset, but found a number)",
		};
		CHECK(Warnings == Expected);
		CHECK(Material->Metallic == 1.0f);
		CHECK(Material->NormalScale == 0.5f);
		CHECK(Material->Roughness == 0.5f);
		CHECK(Material->BaseColor == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		CHECK(Material->BaseColorTexture == nullptr);
		CHECK(Material->OcclusionTexture == nullptr);
	}
}
