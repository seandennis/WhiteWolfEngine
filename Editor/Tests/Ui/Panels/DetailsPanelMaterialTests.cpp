#include "Assets/EditedAssets.h"
#include "Assets/TestAssets.h"
#include "Assets/TestEditedAssets.h"
#include "Core/FileHelper.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestGltfFiles.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// A material that is a file of its own is shown and changed below the Material of the component that uses it,
// tested as a person does it: through FEditorUi, with a mouse and a keyboard, on a surface high enough for every row
// of a mesh component and its material to be in sight.

namespace
{
	constexpr int32 SurfaceWidth = 1280;
	constexpr int32 SurfaceHeight = 1600;

	/** High enough for the rows of several mesh components, one of them with its material's rows, to be in sight. */
	constexpr int32 TallSurfaceHeight = 2600;

	constexpr std::string_view RedPath = "Materials/Red.wwmaterial";

	/** Returns the name of the widget of a property of a component, or of a part of it. */
	std::string Row(int64 ComponentId, std::string_view Property)
	{
		return std::format("Details/{}/{}", ComponentId, Property);
	}

	/** Returns the name of the widget of a property of the material of a component's Material. */
	std::string MaterialRow(int64 ComponentId, std::string_view Property)
	{
		return std::format("Details/{}/Material/Properties/{}", ComponentId, Property);
	}

	/** Returns the name of the row that says that the material of a component's Material is not saved. */
	std::string UnsavedRow(int64 ComponentId)
	{
		return std::format("Details/{}/Material/Unsaved", ComponentId);
	}

	/** Gives the editor a command with the arguments written as JSON text, which the test expects to succeed. */
	void Run(FEditorUiTest& Test, std::string_view CommandName, const std::string& ArgumentText)
	{
		const FCommandResult Result = Test.Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
		INFO(CommandName, ": ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
	}

	/** Adds a static mesh component to an entity, with the material of the given path if it is not empty. */
	int64 AddMeshComponent(FEditorUiTest& Test, int64 EntityId, std::string_view MaterialPath)
	{
		WEntity* const Entity = Test.Editor.GetScene().FindEntityById(EntityId);
		REQUIRE(Entity != nullptr);
		const WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Component != nullptr);
		if (!MaterialPath.empty())
		{
			Run(Test,
				"Object.SetProperty",
				std::format(
					R"({{"Object": {}, "Property": "Material", "Value": "{}"}})", Component->GetId(), MaterialPath));
		}
		return Component->GetId();
	}

	/** Creates an entity and selects it, so that the panel shows it. */
	int64 AddSelectedEntity(FEditorUiTest& Test)
	{
		const WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Wall");
		REQUIRE(Entity != nullptr);
		Test.Editor.SetSelection({Entity->GetId()});
		return Entity->GetId();
	}

	/** Types a number into a field that is otherwise dragged: a click with Ctrl held makes a text field of it. */
	void TypeNumber(FEditorUiTest& Test, const std::string& ItemName, std::string_view Text)
	{
		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(ItemName);
		Test.SetKeyDown(EKey::LeftControl, false);
		Test.Type(Text);
		Test.PressKey(EKey::Enter);
	}

	/** Returns what the last step of the history is called, or nothing if there is none. */
	std::string GetLastStepName(const FEditorUiTest& Test)
	{
		const std::vector<std::string> Names = Test.Editor.GetHistory().GetUndoStepNames();
		return Names.empty() ? std::string() : Names.front();
	}

	/** A project with a material file, and a selected entity whose mesh component uses it. */
	struct FMaterialPanelTest
	{
		explicit FMaterialPanelTest(int32 SurfaceHeightInPixels = SurfaceHeight)
			: Test(SurfaceWidth, SurfaceHeightInPixels)
			, Content(CreateTestProject(Test.Editor, Directory.GetPath()))
		{
			Run(Test, "Asset.CreateMaterial", std::format(R"({{"Path": "{}"}})", RedPath));
			Entity = AddSelectedEntity(Test);
			Component = AddMeshComponent(Test, Entity, RedPath);
			// Rows that come into sight are given a scroll bar a frame later, which moves them.
			Test.DrawFrames(3);
		}

		/** Returns the material, which every component that uses it shares. */
		WMaterial& GetMaterial() const
		{
			return LoadTestMaterial(RedPath);
		}

		/** Returns the text of the material's file. */
		std::string ReadMaterialFile() const
		{
			std::string Text;
			std::string Error;
			const bool bIsRead = FFileHelper::LoadFileToString(Content / "Materials" / "Red.wwmaterial", Text, Error);
			INFO("Error: ", Error);
			REQUIRE(bIsRead);
			return Text;
		}

		FEditorUiTest Test;
		FScopedTempDirectory Directory;
		std::filesystem::path Content;
		int64 Entity = 0;
		int64 Component = 0;
	};
}

TEST_SUITE("FDetailsPanel materials")
{
	TEST_CASE("The values of a material file are rows below the Material of the component that uses it")
	{
		FMaterialPanelTest Panel;
		FEditorUiTest& Test = Panel.Test;

		for (const std::string_view Property : {"BaseColor",
				 "Metallic",
				 "Roughness",
				 "EmissiveColor",
				 "BaseColorTexture/Choose",
				 "MetallicRoughnessTexture/Choose",
				 "NormalTexture/Choose",
				 "NormalScale",
				 "EmissiveTexture/Choose",
				 "OcclusionTexture/Choose",
				 "OcclusionStrength"})
		{
			CAPTURE(std::string(Property));
			CHECK(Test.HasItem(MaterialRow(Panel.Component, Property)));
		}
		// Below the row of the Material itself, and nothing says that anything is not saved.
		CHECK(Test.RequireItem(MaterialRow(Panel.Component, "Metallic")).Top >
			Test.RequireItem(Row(Panel.Component, "Material")).Top);
		CHECK_FALSE(Test.HasItem(UnsavedRow(Panel.Component)));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A number of a material that is dragged is one step, which is not saved until Ctrl+S")
	{
		FMaterialPanelTest Panel;
		FEditorUiTest& Test = Panel.Test;
		const std::string FileText = Panel.ReadMaterialFile();

		Test.DragBy(MaterialRow(Panel.Component, "Roughness"), FVector2D(60.0, 0.0));

		const float Dragged = Panel.GetMaterial().Roughness;
		CHECK(Dragged > 0.5f);
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(GetLastStepName(Test) == "Set Roughness");
		CHECK(Test.Editor.GetHistory().GetUndoStepNames() ==
			std::vector<std::string>{"Set Roughness", "Object.SetProperty"});
		CHECK(Test.HasItem(UnsavedRow(Panel.Component)));
		CHECK(Panel.ReadMaterialFile() == FileText);

		// Undone, the material is what its file says, and the row that said otherwise is gone.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Panel.GetMaterial().Roughness == 0.5f);
		CHECK_FALSE(Test.HasItem(UnsavedRow(Panel.Component)));
		Test.PressKey(EKey::Y, {EKey::LeftControl});
		CHECK(Panel.GetMaterial().Roughness == Dragged);
		CHECK(Test.HasItem(UnsavedRow(Panel.Component)));

		// Ctrl+S writes it, and then asks where to save the scene, which has no file yet.
		Test.PressKey(EKey::S, {EKey::LeftControl});
		const FJsonValue Saved = ParseTestJson(Panel.ReadMaterialFile());
		CHECK(static_cast<float>(Saved.FindMember("Properties")->FindMember("Roughness")->GetNumber()) == Dragged);
		CHECK_FALSE(Test.HasItem(UnsavedRow(Panel.Component)));
		CHECK(Test.Editor.GetEditedAssets().GetUnsaved().empty());
	}

	TEST_CASE("A number typed into a row of a material is given to the material")
	{
		FMaterialPanelTest Panel;
		FEditorUiTest& Test = Panel.Test;

		TypeNumber(Test, MaterialRow(Panel.Component, "Metallic"), "1");

		CHECK(Panel.GetMaterial().Metallic == 1.0f);
		CHECK(GetLastStepName(Test) == "Set Metallic");
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The arrow beside a changed value of a material puts back what a new material has")
	{
		FMaterialPanelTest Panel;
		FEditorUiTest& Test = Panel.Test;
		Run(Test,
			"Asset.SetProperty",
			std::format(R"({{"Asset": "{}", "Property": "Metallic", "Value": 1}})", RedPath));
		Test.DrawFrames(2);
		CHECK_FALSE(Test.HasItem(MaterialRow(Panel.Component, "Roughness/Reset")));

		Test.Click(MaterialRow(Panel.Component, "Metallic/Reset"));

		CHECK(Panel.GetMaterial().Metallic == 0.0f);
		CHECK(GetLastStepName(Test) == "Reset Metallic");
		CHECK_FALSE(Test.HasItem(MaterialRow(Panel.Component, "Metallic/Reset")));
	}

	TEST_CASE("Two components that use one material both show it, and a change from one is seen in both")
	{
		FMaterialPanelTest Panel(TallSurfaceHeight);
		FEditorUiTest& Test = Panel.Test;
		const int64 Second = AddMeshComponent(Test, Panel.Entity, RedPath);
		Test.DrawFrames(3);
		REQUIRE(Test.HasItem(MaterialRow(Second, "Metallic")));

		TypeNumber(Test, MaterialRow(Panel.Component, "Metallic"), "1");

		CHECK(Panel.GetMaterial().Metallic == 1.0f);
		CHECK(Test.HasItem(UnsavedRow(Panel.Component)));
		CHECK(Test.HasItem(UnsavedRow(Second)));
	}

	TEST_CASE("A Material that is none, of a model, or made in code shows no rows")
	{
		FMaterialPanelTest Panel(TallSurfaceHeight);
		FEditorUiTest& Test = Panel.Test;
		const std::filesystem::path Model = Panel.Directory.GetPath() / "Square.gltf";
		WriteTestGltfFile(Model, FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		Run(Test,
			"Asset.Import",
			std::format(R"({{"Source": {}}})", WriteTestJson(FJsonValue(FFileHelper::PathToUtf8(Model)))));
		const int64 None = AddMeshComponent(Test, Panel.Entity, std::string_view());
		const int64 OfModel = AddMeshComponent(Test, Panel.Entity, "Square.gltf#Materials/0");
		const int64 MadeInCode = AddMeshComponent(Test, Panel.Entity, std::string_view());
		WStaticMeshComponent* const Component =
			Cast<WStaticMeshComponent>(Test.Editor.GetScene().FindComponentById(MadeInCode));
		REQUIRE(Component != nullptr);
		Component->Material = NewObject<WMaterial>();
		Test.DrawFrames(3);

		for (const int64 Id : {None, OfModel, MadeInCode})
		{
			CAPTURE(Id);
			CHECK(Test.HasItem(Row(Id, "Material")));
			CHECK_FALSE(Test.HasItem(MaterialRow(Id, "Metallic")));
		}
	}

	TEST_CASE("A name that its column cuts off is shown whole while the mouse is over it")
	{
		FMaterialPanelTest Panel;
		FEditorUiTest& Test = Panel.Test;
		const std::string Long = MaterialRow(Panel.Component, "MetallicRoughnessTexture") + "/Label";
		const std::string Short = MaterialRow(Panel.Component, "Metallic") + "/Label";

		// The mouse rests at the start of the name, which is in its column however long the name is.
		const auto Hover = [&Test](const std::string& Label)
		{
			const FUiRectangle Rectangle = Test.RequireItem(Label);
			Test.MoveMouse(FVector2D(Rectangle.Left + 4.0, (Rectangle.Top + Rectangle.Bottom) * 0.5));
			// Long enough for the mouse to have rested, which a tooltip waits for.
			Test.DrawFrames(30);
		};

		CHECK_FALSE(Test.HasItem(Long + "/Whole"));
		Hover(Long);
		CHECK(Test.HasItem(Long + "/Whole"));
		Hover(Short);
		CHECK_FALSE(Test.HasItem(Short + "/Whole"));
		CHECK_FALSE(Test.HasItem(Long + "/Whole"));
	}

	TEST_CASE("Searching for a value of a material shows the component's Material with the material's rows")
	{
		FMaterialPanelTest Panel;
		FEditorUiTest& Test = Panel.Test;

		Test.Click("Details/Search");
		Test.Type("rough");
		Test.DrawFrames(3);

		CHECK(Test.HasItem(Row(Panel.Component, "Material")));
		CHECK(Test.HasItem(MaterialRow(Panel.Component, "Roughness")));
		CHECK_FALSE(Test.HasItem(Row(Panel.Component, "Mesh")));
	}
}
