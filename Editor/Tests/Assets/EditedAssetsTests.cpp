#include "Assets/EditedAssets.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPath.h"
#include "Assets/TestAssets.h"
#include "Assets/TestEditedAssets.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialSerializer.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/Texture.h"
#include "TestEditorObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	constexpr std::string_view RedPath = "Materials/Red.wwmaterial";

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

TEST_SUITE("FEditedAssets")
{
	TEST_CASE("SetProperty changes the object that was loaded, and keeps it while nothing else refers to it")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath), R"({"BaseColor": [1, 0, 0, 1]})");
		const std::string FileText = ReadContentFile(Content, RedPath);
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		const TWeakObjectPtr<WMaterial> Weak = &Material;
		const FEditedAssets& Assets = Editor.GetEditedAssets();
		CHECK_FALSE(Assets.IsKept(RedPath));

		ChangeMaterial(Editor, Material, "Metallic", "1.0");

		CHECK(Material.Metallic == 1.0f);
		CHECK(Assets.IsKept(RedPath));
		CHECK(Assets.HasUnsavedChanges(RedPath));
		CHECK(Assets.GetUnsaved() == std::vector<std::string>{std::string(RedPath)});
		// Nothing but the editor refers to it: it is kept all the same, and is what the path gives.
		CollectGarbage();
		REQUIRE(Weak.IsValid());
		CHECK(&LoadTestMaterial(RedPath) == &Material);
		CHECK(Material.Metallic == 1.0f);
		// The file is written only when the material is saved.
		CHECK(ReadContentFile(Content, RedPath) == FileText);
	}

	TEST_CASE("A value that is refused leaves the asset as it was, and keeps nothing")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath), R"({"Metallic": 0.5})");
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		const TWeakObjectPtr<WMaterial> Weak = &Material;
		const std::string Before = FMaterialSerializer::SaveToText(Material);
		std::string PropertyName;
		std::string ValueText;
		std::string Expected;

		SUBCASE("Text for a number")
		{
			PropertyName = "Metallic";
			ValueText = R"("shiny")";
			Expected = "expected a number but found a string";
		}
		SUBCASE("A texture whose file is not there")
		{
			PropertyName = "BaseColorTexture";
			ValueText = R"("Textures/Gone.png")";
			Expected = "the content directory has no file \"Textures/Gone.png\"";
		}
		SUBCASE("A mesh for a texture")
		{
			PropertyName = "BaseColorTexture";
			ValueText = R"("Engine/Shapes#Cube")";
			Expected = "the reference is to a WStaticMesh, which is not a WTexture";
		}

		std::string Error;
		CHECK_FALSE(Editor.GetEditedAssets().SetProperty(
			Material, FindMaterialProperty(PropertyName), ParseTestJson(ValueText), Error));

		CHECK(Error == Expected);
		CHECK(FMaterialSerializer::SaveToText(Material) == Before);
		CHECK_FALSE(Editor.GetEditedAssets().IsKept(RedPath));
		CollectGarbage();
		CHECK_FALSE(Weak.IsValid());
	}

	TEST_CASE("The value that a property has already keeps nothing")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath), R"({"Metallic": 0.5})");
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);

		ChangeMaterial(Editor, Material, "Metallic", "0.5");

		CHECK_FALSE(Editor.GetEditedAssets().IsKept(RedPath));
		CHECK(Editor.GetEditedAssets().GetUnsaved().empty());
	}

	TEST_CASE("Save writes the changes of an asset to its file, and writes nothing when there is nothing to write")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FEditedAssets& Assets = Editor.GetEditedAssets();
		WMaterial& Material = LoadTestMaterial(RedPath);
		ChangeMaterial(Editor, Material, "Roughness", "0.25");
		bool bWritten = false;
		std::string Error;

		REQUIRE(Assets.Save(Material, bWritten, Error));

		CHECK(bWritten);
		CHECK(ReadContentFile(Content, RedPath) == FMaterialSerializer::SaveToText(Material));
		CHECK_FALSE(Assets.HasUnsavedChanges(RedPath));
		CHECK(Assets.GetUnsaved().empty());
		// It stays kept: the history can still change it.
		CHECK(Assets.IsKept(RedPath));

		// Saved again, it has nothing to write, and leaves the file alone.
		Content.WriteText(std::string(RedPath), "Not to be overwritten");
		REQUIRE(Assets.Save(Material, bWritten, Error));
		CHECK_FALSE(bWritten);
		CHECK(ReadContentFile(Content, RedPath) == "Not to be overwritten");
	}

	TEST_CASE("Save of an asset that the editor does not keep writes it only if its file is gone")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath), R"({"Metallic": 1.0})");
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FEditedAssets& Assets = Editor.GetEditedAssets();
		WMaterial& Material = LoadTestMaterial(RedPath);
		const std::string FileText = ReadContentFile(Content, RedPath);
		bool bWritten = true;
		std::string Error;

		REQUIRE(Assets.Save(Material, bWritten, Error));
		CHECK_FALSE(bWritten);
		CHECK(ReadContentFile(Content, RedPath) == FileText);

		std::filesystem::remove(Content.GetPath() / FFileHelper::PathFromUtf8(RedPath));
		REQUIRE(Assets.Save(Material, bWritten, Error));
		CHECK(bWritten);
		CHECK(ReadContentFile(Content, RedPath) == FMaterialSerializer::SaveToText(Material));
		CHECK_FALSE(Assets.IsKept(RedPath));
	}

	TEST_CASE("Save says why it cannot write the file, and the asset stays unsaved")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FEditedAssets& Assets = Editor.GetEditedAssets();
		WMaterial& Material = LoadTestMaterial(RedPath);
		ChangeMaterial(Editor, Material, "Metallic", "1.0");
		// A folder with something in it has taken the file's place.
		std::filesystem::remove(Content.GetPath() / FFileHelper::PathFromUtf8(RedPath));
		Content.WriteText(std::string(RedPath) + "/Inside.txt", "In the way");
		bool bWritten = true;
		std::string Error;

		CHECK_FALSE(Assets.Save(Material, bWritten, Error));

		CHECK_FALSE(bWritten);
		CHECK(Error.starts_with("cannot replace '"));
		CHECK(Assets.HasUnsavedChanges(RedPath));
		CHECK(Assets.GetUnsaved() == std::vector<std::string>{std::string(RedPath)});
	}

	TEST_CASE("Save says that there is nowhere to write without a content directory")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		WMaterial& Material = LoadTestMaterial(RedPath);
		std::filesystem::remove(Content.GetPath() / FFileHelper::PathFromUtf8(RedPath));
		FAssetManager::SetContentDirectory(std::filesystem::path());
		bool bWritten = true;
		std::string Error;

		CHECK_FALSE(Editor.GetEditedAssets().Save(Material, bWritten, Error));

		CHECK_FALSE(bWritten);
		CHECK(Error == "there is no content directory to write it to");
	}

	TEST_CASE("Only a material file can be changed")
	{
		const FScopedContentDirectory Content;
		const FScopedEditorTest EditorTest;
		Content.WriteMaterial(std::string(RedPath));
		Content.WriteSquare("Models/Square.gltf");
		Content.WritePicture("Textures/Wood.png");
		Content.WriteEnvironment("Skies/Noon.hdr");
		std::string Error;

		const auto CanChangePath = [](std::string_view Text)
		{
			std::string ParseError;
			const std::optional<FAssetPath> Path = FAssetPath::Parse(Text, ParseError);
			REQUIRE(Path.has_value());
			return FEditedAssets::CanChange(*Path);
		};
		CHECK(CanChangePath("Materials/Red.wwmaterial"));
		CHECK(CanChangePath("Materials/Loud.WWMATERIAL"));
		CHECK_FALSE(CanChangePath("Materials/Red.wwmaterial#Key"));
		CHECK_FALSE(CanChangePath("Models/Square.gltf#Materials/0"));
		CHECK_FALSE(CanChangePath("Textures/Wood.png"));
		CHECK_FALSE(CanChangePath("Skies/Noon.hdr"));
		CHECK_FALSE(CanChangePath("Engine/Shapes#Cube"));

		CHECK(FEditedAssets::CanChange(LoadTestMaterial(RedPath)));
		for (const std::string_view Other :
			{"Models/Square.gltf#Materials/0", "Textures/Wood.png", "Skies/Noon.hdr", "Engine/Shapes#Cube"})
		{
			const WAsset* const Asset = FAssetManager::Load(Other, Error);
			REQUIRE(Asset != nullptr);
			CHECK_FALSE(FEditedAssets::CanChange(*Asset));
		}
		CHECK_FALSE(FEditedAssets::CanChange(*NewObject<WMaterial>()));
	}

	TEST_CASE("Changing an asset that cannot be changed, or a property that is not saved, is a mistake in the code")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		Content.WriteSquare("Models/Square.gltf");
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FScopedLogCapture Capture;
		FEditedAssets& Assets = Editor.GetEditedAssets();
		std::string Error;

		WMaterial& ModelMaterial = LoadTestMaterial("Models/Square.gltf#Materials/0");
		CHECK_THROWS_AS(Assets.SetProperty(ModelMaterial, FindMaterialProperty("Metallic"), FJsonValue(1.0), Error),
			FFatalErrorException);
		bool bWritten = false;
		CHECK_THROWS_AS(Assets.Save(ModelMaterial, bWritten, Error), FFatalErrorException);

		WMaterial& Material = LoadTestMaterial(RedPath);
		CHECK_THROWS_AS(
			Assets.SetProperty(Material, FindMaterialProperty("Package"), FJsonValue(), Error), FFatalErrorException);
		CHECK_FALSE(Assets.IsKept(RedPath));
	}

	TEST_CASE("An asset that the editor keeps and something else destroyed is a mistake in the code")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		ChangeMaterial(Editor, Material, "Metallic", "1.0");
		const FScopedLogCapture Capture;

		Material.Destroy();

		CHECK_THROWS_AS(Editor.Undo(), FFatalErrorException);
	}

	TEST_CASE("The end of the editor gives unsaved changes up, with one warning that names them, and keeps nothing")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath), R"({"Metallic": 0.5})");
		Content.WriteMaterial("Materials/Saved.wwmaterial");
		Content.WriteMaterial("Materials/Wood.wwmaterial");
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		TWeakObjectPtr<WMaterial> Red;
		TWeakObjectPtr<WMaterial> Saved;
		TWeakObjectPtr<WMaterial> Wood;
		{
			FEditor Editor;
			Red = &LoadTestMaterial(RedPath);
			Saved = &LoadTestMaterial("Materials/Saved.wwmaterial");
			Wood = &LoadTestMaterial("Materials/Wood.wwmaterial");
			ChangeMaterial(Editor, *Red.Get(), "Metallic", "1.0");
			ChangeMaterial(Editor, *Saved.Get(), "Metallic", "1.0");
			ChangeMaterial(Editor, *Wood.Get(), "Roughness", "0.0");
			bool bWritten = false;
			std::string Error;
			REQUIRE(Editor.GetEditedAssets().Save(*Saved.Get(), bWritten, Error));
		}

		CHECK(GetLoggedWarnings(Capture) ==
			std::vector<std::string>{
				"Changes to these assets were not saved and are given up: Materials/Red.wwmaterial, "
				"Materials/Wood.wwmaterial"});
		// What is not kept is what its file says, until the collector takes it.
		REQUIRE(Red.IsValid());
		CHECK(Red.Get()->Metallic == 0.5f);
		REQUIRE(Wood.IsValid());
		CHECK(Wood.Get()->Roughness == 0.5f);
		REQUIRE(Saved.IsValid());
		CHECK(Saved.Get()->Metallic == 1.0f);
		CollectGarbage();
		CHECK_FALSE(Red.IsValid());
		CHECK_FALSE(Saved.IsValid());
		CHECK_FALSE(Wood.IsValid());
	}

	TEST_CASE("The end of an editor whose changes are saved gives no warning")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		{
			FEditor Editor;
			WMaterial& Material = LoadTestMaterial(RedPath);
			ChangeMaterial(Editor, Material, "Metallic", "1.0");
			bool bWritten = false;
			std::string Error;
			REQUIRE(Editor.GetEditedAssets().Save(Material, bWritten, Error));
		}

		CHECK(GetLoggedWarnings(Capture).empty());
	}
}
