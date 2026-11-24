#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <filesystem>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// The Details panel is tested as a person uses it: through FEditorUi, with a mouse and a keyboard, on a surface
// high enough for every row of the component that these tests show to be in sight.

namespace
{
	constexpr int32 SurfaceWidth = 1280;
	constexpr int32 SurfaceHeight = 1500;

	/** Returns the name of the widget of a property of an entity or component, or of a part of it. */
	std::string Row(int64 ObjectId, std::string_view Property)
	{
		return std::format("Details/{}/{}", ObjectId, Property);
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

	/** Returns the pixels of the surface that an item covers, row after row: what the item looks like. */
	std::vector<FColor> ReadItem(const FEditorUiTest& Test, const std::string& ItemName)
	{
		const FUiRectangle Rectangle = Test.RequireItem(ItemName);
		const FImage Picture = Test.ReadPicture();
		std::vector<FColor> Pixels;
		for (int32 Y = static_cast<int32>(Rectangle.Top); Y < static_cast<int32>(Rectangle.Bottom); ++Y)
		{
			for (int32 X = static_cast<int32>(Rectangle.Left); X < static_cast<int32>(Rectangle.Right); ++X)
			{
				Pixels.push_back(Picture.GetPixel(X, Y));
			}
		}
		return Pixels;
	}

	/** Returns how many pixels of an item are as light as letters are: how much is written on it. */
	int32 CountInk(const FEditorUiTest& Test, const std::string& ItemName)
	{
		constexpr uint8 LeastInk = 150;
		int32 Count = 0;
		for (const FColor& Pixel : ReadItem(Test, ItemName))
		{
			Count += Pixel.R > LeastInk ? 1 : 0;
		}
		return Count;
	}

	/** Creates an entity in the editor's scene, selects it and shows it in the panel. */
	int64 AddSelectedEntity(FEditorUiTest& Test, const char* Name)
	{
		const WEntity* const Entity = Test.Editor.GetScene().CreateEntity(Name);
		REQUIRE(Entity != nullptr);
		Test.Editor.SetSelection({Entity->GetId()});
		Test.DrawFrame();
		return Entity->GetId();
	}

	/** An entity that is selected, and its component with a property of every kind. */
	struct FGadgetIds
	{
		int64 Entity = 0;
		int64 Component = 0;
	};

	FGadgetIds AddSelectedGadget(FEditorUiTest& Test)
	{
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Gadget");
		REQUIRE(Entity != nullptr);
		const WEditorTestGadgetComponent* const Component = Entity->AddComponent<WEditorTestGadgetComponent>();
		REQUIRE(Component != nullptr);
		Test.Editor.SetSelection({Entity->GetId()});
		Test.DrawFrame();
		return {Entity->GetId(), Component->GetId()};
	}

	/** Returns the gadget as it is in the editor's scene now; an undo replaces the scene's objects. */
	const WEditorTestGadgetComponent& GetGadget(const FEditorUiTest& Test, const FGadgetIds& Ids)
	{
		const WEditorTestGadgetComponent* const Gadget =
			Cast<WEditorTestGadgetComponent>(Test.Editor.GetScene().FindComponentById(Ids.Component));
		REQUIRE(Gadget != nullptr);
		return *Gadget;
	}

	const WEntity& GetEntity(const FEditorUiTest& Test, int64 EntityId)
	{
		const WEntity* const Entity = Test.Editor.GetScene().FindEntityById(EntityId);
		REQUIRE(Entity != nullptr);
		return *Entity;
	}

	/** Returns what the last step of the history is called, or nothing if there is none. */
	std::string GetLastStepName(const FEditorUiTest& Test)
	{
		const std::vector<std::string> Names = Test.Editor.GetHistory().GetUndoStepNames();
		return Names.empty() ? std::string() : Names.front();
	}

	/** Checks that two unit quaternions describe the same rotation; Q and -Q are the same rotation. */
	void CheckSameRotation(const FQuat& Actual, const FQuat& Expected)
	{
		constexpr float Tolerance = 1.0e-5f;
		CHECK(std::abs(glm::dot(Actual, Expected)) >= 1.0f - Tolerance);
	}
}

TEST_SUITE("FDetailsPanel")
{
	TEST_CASE("The panel says that nothing is selected, and shows the entity that was selected last")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		Test.DrawFrame();

		CHECK(Test.HasItem("Details/NothingSelected"));
		CHECK_FALSE(Test.HasItem("Details/Name"));

		Test.Click(std::format("Outliner/{}", Player));
		CHECK_FALSE(Test.HasItem("Details/NothingSelected"));
		CHECK(Test.HasItem("Details/Name"));
		CHECK(Test.HasItem("Details/Transform"));
		CHECK(Test.HasItem(Row(Player, "RelativeTransform/Location/X")));

		// Of several, the one that was selected last is the one whose values are shown.
		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(std::format("Outliner/{}", Ground));
		Test.SetKeyDown(EKey::LeftControl, false);
		CHECK(Test.HasItem(Row(Ground, "RelativeTransform/Location/X")));
		CHECK_FALSE(Test.HasItem(Row(Player, "RelativeTransform/Location/X")));

		Test.Click("Outliner/Empty");
		CHECK(Test.HasItem("Details/NothingSelected"));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The name of the entity is typed over where it stands")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");

		Test.Click("Details/Name");
		// The old name is selected, so what is typed takes its place.
		Test.Type("Hero");
		// Until the user is done, the entity keeps its name.
		CHECK(GetEntity(Test, Player).GetName() == "Player");
		Test.PressKey(EKey::Enter);

		CHECK(GetEntity(Test, Player).GetName() == "Hero");
		CHECK(GetLastStepName(Test) == "Rename");

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetName() == "Player");
	}

	TEST_CASE("Escape leaves the entity the name it had")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");
		const std::string StepBefore = GetLastStepName(Test);

		Test.Click("Details/Name");
		Test.Type("Villain");
		Test.PressKey(EKey::Escape);

		CHECK(GetEntity(Test, Player).GetName() == "Player");
		CHECK(GetLastStepName(Test) == StepBefore);
	}

	TEST_CASE("A name that is being typed goes to the entity it was typed for when another is selected")
	{
		FEditorUiTest Test;
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		const int64 Player = AddSelectedEntity(Test, "Player");

		Test.Click("Details/Name");
		Test.Type("Hero");
		// The click that selects the other entity is also what ends the typing.
		Test.Click(std::format("Outliner/{}", Ground));

		CHECK(GetEntity(Test, Player).GetName() == "Hero");
		CHECK(GetEntity(Test, Ground).GetName() == "Ground");
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Ground});
	}

	TEST_CASE("A number is typed after a click with Ctrl held, and is the entity's once the user is done")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");

		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(Row(Player, "RelativeTransform/Location/X"));
		Test.SetKeyDown(EKey::LeftControl, false);
		Test.Type("12.5");

		// Until Enter, what is typed is in the field only.
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == FVector(0.0));
		CHECK_FALSE(Test.Editor.IsStepOpen());
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == FVector(12.5, 0.0, 0.0));
		CHECK(GetLastStepName(Test) == "Set Transform");

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == FVector(0.0));
		Test.PressKey(EKey::Y, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == FVector(12.5, 0.0, 0.0));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A number that is dragged goes through many values in one step")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");

		Test.DragBy(Row(Player, "RelativeTransform/Location/Y"), FVector2D(60.0, 0.0));

		const FVector Dragged = GetEntity(Test, Player).GetRelativeTransform().Location;
		CHECK(Dragged.x == 0.0);
		CHECK(Dragged.y > 1.0);
		CHECK(Dragged.y <= 3.0);
		CHECK(Dragged.z == 0.0);
		// What is dragged comes out as a number that can be read, without the dust of adding many small steps.
		CHECK(std::round(Dragged.y * 1000.0) == doctest::Approx(Dragged.y * 1000.0));
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(GetLastStepName(Test) == "Set Transform");

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == FVector(0.0));
		Test.PressKey(EKey::Y, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == Dragged);
	}

	TEST_CASE("A number that reads as zero is shown as zero, whatever sign it has")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");
		const std::string Field = Row(Player, "RelativeTransform/Location/X");
		const std::vector<FColor> ShownAtZero = ReadItem(Test, Field);

		// Returns what the field shows once the entity is that far along X.
		const auto ShowAt = [&Test, &Field, Player](double X)
		{
			WEntity* const Entity = Test.Editor.GetScene().FindEntityById(Player);
			REQUIRE(Entity != nullptr);
			FTransform Transform;
			Transform.Location = FVector(X, 0.0, 0.0);
			Entity->SetRelativeTransform(Transform);
			Test.DrawFrame();
			return ReadItem(Test, Field);
		};

		// Closer to zero than the digits that are shown can say, from below and from above, and a zero that has
		// a minus sign of its own: none of them reads "-0".
		CHECK(ShowAt(-0.0004) == ShownAtZero);
		CHECK(ShowAt(0.0004) == ShownAtZero);
		CHECK(ShowAt(-0.0) == ShownAtZero);

		// What the digits can say is shown as it is.
		CHECK_FALSE(ShowAt(-0.001) == ShownAtZero);
		CHECK_FALSE(ShowAt(-1.0) == ShownAtZero);
		CHECK(ShowAt(0.0) == ShownAtZero);
	}

	TEST_CASE("A zero that is typed with a minus sign is the entity's without one")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");
		const std::string Field = Row(Player, "RelativeTransform/Location/Y");
		TypeNumber(Test, Field, "5");
		REQUIRE(GetEntity(Test, Player).GetRelativeTransform().Location.y == 5.0);

		TypeNumber(Test, Field, "-0");

		const double Y = GetEntity(Test, Player).GetRelativeTransform().Location.y;
		CHECK(Y == 0.0);
		CHECK_FALSE(std::signbit(Y));
	}

	TEST_CASE("The arrow beside a part of the transform puts that part back, and is there only if it is changed")
	{
		FEditorUiTest Test;
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Player");
		FTransform Transform;
		Transform.Location = FVector(1.0, 2.0, 3.0);
		Transform.Scale = FVector3f(2.0f);
		Entity->SetRelativeTransform(Transform);
		const int64 Player = Entity->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();

		CHECK(Test.HasItem(Row(Player, "RelativeTransform/Location/Reset")));
		CHECK_FALSE(Test.HasItem(Row(Player, "RelativeTransform/Rotation/Reset")));
		CHECK(Test.HasItem(Row(Player, "RelativeTransform/Scale/Reset")));

		Test.Click(Row(Player, "RelativeTransform/Location/Reset"));

		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location == FVector(0.0));
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Scale == FVector3f(2.0f));
		CHECK(GetLastStepName(Test) == "Reset Transform");
		CHECK_FALSE(Test.HasItem(Row(Player, "RelativeTransform/Location/Reset")));
		CHECK(Test.HasItem(Row(Player, "RelativeTransform/Scale/Reset")));
	}

	TEST_CASE("The angles of the rotation turn the entity")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");

		TypeNumber(Test, Row(Player, "RelativeTransform/Rotation/Y"), "90");

		FRotator Expected;
		Expected.Yaw = 90.0f;
		CheckSameRotation(GetEntity(Test, Player).GetRelativeTransform().Rotation, Expected.ToQuat());

		// The other angles are kept: the rotation is made of all three.
		TypeNumber(Test, Row(Player, "RelativeTransform/Rotation/X"), "30");
		Expected.Pitch = 30.0f;
		CheckSameRotation(GetEntity(Test, Player).GetRelativeTransform().Rotation, Expected.ToQuat());
		CHECK(Test.HasItem(Row(Player, "RelativeTransform/Rotation/Reset")));
	}

	TEST_CASE("An angle that is dragged past straight up goes on, and does not jump back")
	{
		FEditorUiTest Test;
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Player");
		FRotator Tilted;
		Tilted.Pitch = 80.0f;
		FTransform Transform;
		Transform.Rotation = Tilted.ToQuat();
		Entity->SetRelativeTransform(Transform);
		const int64 Player = Entity->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();

		// Half a degree for each pixel: from 80 degrees to more than 90, where the angles of the rotation that
		// results read differently. The angle that is held is the one that goes on.
		Test.DragBy(Row(Player, "RelativeTransform/Rotation/X"), FVector2D(60.0, 0.0));

		const FRotator Reached = FRotator::FromQuat(GetEntity(Test, Player).GetRelativeTransform().Rotation);
		// Tilted over backwards: seen as angles in range, that is a tilt of less than 90 with a half turn of each
		// of the other two.
		CHECK(Reached.Pitch < 80.0f);
		CHECK(std::abs(Reached.Yaw) == doctest::Approx(180.0f).epsilon(0.001));
		CHECK(std::abs(Reached.Roll) == doctest::Approx(180.0f).epsilon(0.001));
	}

	TEST_CASE("A tick box sets a bool, and the arrow beside it puts back what a new component has")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);
		REQUIRE(GetGadget(Test, Ids).bEnabled);
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "bEnabled/Reset")));

		Test.Click(Row(Ids.Component, "bEnabled"));

		CHECK_FALSE(GetGadget(Test, Ids).bEnabled);
		CHECK(GetLastStepName(Test) == "Set Enabled");
		CHECK_FALSE(Test.Editor.IsStepOpen());
		REQUIRE(Test.HasItem(Row(Ids.Component, "bEnabled/Reset")));

		Test.Click(Row(Ids.Component, "bEnabled/Reset"));

		CHECK(GetGadget(Test, Ids).bEnabled);
		CHECK(GetLastStepName(Test) == "Reset Enabled");
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "bEnabled/Reset")));

		// Two steps, each undone by itself.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK_FALSE(GetGadget(Test, Ids).bEnabled);
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetGadget(Test, Ids).bEnabled);
	}

	TEST_CASE("Whole numbers and numbers with a point are typed into their fields")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);

		TypeNumber(Test, Row(Ids.Component, "Level"), "7");
		CHECK(GetGadget(Test, Ids).Level == 7);
		CHECK(GetLastStepName(Test) == "Set Level");

		TypeNumber(Test, Row(Ids.Component, "Speed"), "2.25");
		CHECK(GetGadget(Test, Ids).Speed == 2.25f);
		CHECK(GetLastStepName(Test) == "Set Speed");

		// What is no number is given to nothing.
		TypeNumber(Test, Row(Ids.Component, "Speed"), "nan");
		CHECK(GetGadget(Test, Ids).Speed == 2.25f);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A text is typed into its field, and Escape leaves it as it was")
	{
		FEditorUiTest Test;
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Sign");
		const int64 Component = Entity->AddComponent<WEditorTestComponent>()->GetId();
		Test.Editor.SetSelection({Entity->GetId()});
		Test.DrawFrame();
		const auto GetLabel = [&Test, Component]()
		{
			return Cast<WEditorTestComponent>(Test.Editor.GetScene().FindComponentById(Component))->Label;
		};
		REQUIRE(GetLabel() == "Unnamed");

		Test.Click(Row(Component, "Label"));
		Test.Type("Exit");
		CHECK(GetLabel() == "Unnamed");
		Test.PressKey(EKey::Enter);

		CHECK(GetLabel() == "Exit");
		CHECK(GetLastStepName(Test) == "Set Label");

		Test.Click(Row(Component, "Label"));
		Test.Type("Entrance");
		Test.PressKey(EKey::Escape);
		CHECK(GetLabel() == "Exit");
	}

	TEST_CASE("A vector is changed by its parts")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);

		TypeNumber(Test, Row(Ids.Component, "Offset/Y"), "2.5");
		CHECK(GetGadget(Test, Ids).Offset == FVector(0.0, 2.5, 0.0));
		CHECK(GetLastStepName(Test) == "Set Offset");

		TypeNumber(Test, Row(Ids.Component, "Direction/X"), "0.5");
		CHECK(GetGadget(Test, Ids).Direction == FVector3f(0.5f, 0.0f, -1.0f));

		// A rotation that is a property by itself is three angles, like that of the entity.
		TypeNumber(Test, Row(Ids.Component, "Facing/Z"), "45");
		FRotator Expected;
		Expected.Roll = 45.0f;
		CheckSameRotation(GetGadget(Test, Ids).Facing, Expected.ToQuat());
	}

	TEST_CASE("A transform that is a property has a row for each of its parts, and an arrow for all of it")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Anchor/Reset")));

		TypeNumber(Test, Row(Ids.Component, "Anchor/Location/X"), "4");
		TypeNumber(Test, Row(Ids.Component, "Anchor/Scale/Z"), "3");

		CHECK(GetGadget(Test, Ids).Anchor.Location == FVector(4.0, 0.0, 0.0));
		CHECK(GetGadget(Test, Ids).Anchor.Scale == FVector3f(1.0f, 1.0f, 3.0f));
		CHECK(GetLastStepName(Test) == "Set Anchor");

		// The arrow of a part puts that part back, and the arrow of the transform all of it.
		Test.Click(Row(Ids.Component, "Anchor/Scale/Reset"));
		CHECK(GetGadget(Test, Ids).Anchor.Scale == FVector3f(1.0f));
		CHECK(GetGadget(Test, Ids).Anchor.Location == FVector(4.0, 0.0, 0.0));

		Test.Click(Row(Ids.Component, "Anchor/Reset"));
		CHECK(GetGadget(Test, Ids).Anchor == FTransform());
		CHECK(GetLastStepName(Test) == "Reset Anchor");
	}

	TEST_CASE("A colour is picked in the window that its button opens, or typed as amounts of light")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Tint/Picker")));

		Test.Click(Row(Ids.Component, "Tint"));
		REQUIRE(Test.HasItem(Row(Ids.Component, "Tint/Picker")));

		// The numbers are the amounts of light, which may be more than white's.
		TypeNumber(Test, Row(Ids.Component, "Tint/G"), "4");
		CHECK(GetGadget(Test, Ids).Tint == FLinearColor(1.0f, 4.0f, 0.25f, 1.0f));
		CHECK(GetLastStepName(Test) == "Set Tint");

		// There is no less light than none.
		TypeNumber(Test, Row(Ids.Component, "Tint/B"), "-1");
		CHECK(GetGadget(Test, Ids).Tint == FLinearColor(1.0f, 4.0f, 0.0f, 1.0f));

		// A click in the picker picks another colour, of no more than white's light, and leaves alpha alone.
		Test.Click(Row(Ids.Component, "Tint/Picker"));
		const FLinearColor Picked = GetGadget(Test, Ids).Tint;
		CHECK_FALSE(Picked == FLinearColor(1.0f, 4.0f, 0.0f, 1.0f));
		CHECK(Picked.G <= 1.0f);
		CHECK(Picked.A == 1.0f);
		CHECK_FALSE(Test.Editor.IsStepOpen());

		Test.PressKey(EKey::Escape);
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Tint/Picker")));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A reference to an entity or to a component is chosen from a list of those of the scene")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Lamp = Scene.CreateEntity("Lamp");
		const int64 LampId = Lamp->GetId();
		const int64 Partner = Lamp->AddComponent<WEditorTestDerivedComponent>()->GetId();
		const int64 Light = Lamp->AddComponent<WPointLightComponent>()->GetId();
		const FGadgetIds Ids = AddSelectedGadget(Test);

		Test.Click(Row(Ids.Component, "Watched"));
		REQUIRE(Test.HasItem(Row(Ids.Component, std::format("Watched/{}", LampId))));
		Test.Click(Row(Ids.Component, std::format("Watched/{}", LampId)));

		CHECK(GetGadget(Test, Ids).Watched.Get() == &GetEntity(Test, LampId));
		CHECK(GetLastStepName(Test) == "Set Watched");

		// The list of a reference to a component has the components of its class, and of classes derived from it.
		Test.Click(Row(Ids.Component, "Partner"));
		REQUIRE(Test.HasItem(Row(Ids.Component, std::format("Partner/{}", Partner))));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, std::format("Partner/{}", Light))));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, std::format("Partner/{}", Ids.Component))));
		Test.Click(Row(Ids.Component, std::format("Partner/{}", Partner)));

		REQUIRE(GetGadget(Test, Ids).Partner != nullptr);
		CHECK(GetGadget(Test, Ids).Partner->GetId() == Partner);

		// "None" refers to nothing again.
		Test.Click(Row(Ids.Component, "Watched"));
		Test.Click(Row(Ids.Component, "Watched/None"));
		CHECK(GetGadget(Test, Ids).Watched.Get() == nullptr);
		CHECK(GetGadget(Test, Ids).Partner != nullptr);
	}

	TEST_CASE("The name of an entity is written whole in the list of a reference and in its field")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		WScene& Scene = Test.Editor.GetScene();
		// Two number signs end the text of a row for the library that lays the list out, and are no more than two
		// letters of a name.
		const int64 Lamp = Scene.CreateEntity("Lamp")->GetId();
		const int64 LampTwo = Scene.CreateEntity("Lamp##Two")->GetId();
		const FGadgetIds Ids = AddSelectedGadget(Test);
		const std::string Field = Row(Ids.Component, "Watched");
		const std::string ShortRow = Row(Ids.Component, std::format("Watched/{}", Lamp));
		const std::string LongRow = Row(Ids.Component, std::format("Watched/{}", LampTwo));

		Test.Click(Field);
		REQUIRE(Test.HasItem(ShortRow));
		REQUIRE(Test.HasItem(LongRow));
		// The longer name is written with more ink than the shorter one, which it would not be if it ended after
		// "Lamp".
		CHECK(CountInk(Test, LongRow) > CountInk(Test, ShortRow));

		// And so it is in the field, once it is chosen.
		Test.Click(ShortRow);
		REQUIRE(GetGadget(Test, Ids).Watched.Get() == &GetEntity(Test, Lamp));
		const int32 ShortInk = CountInk(Test, Field);
		Test.Click(Field);
		Test.Click(LongRow);
		REQUIRE(GetGadget(Test, Ids).Watched.Get() == &GetEntity(Test, LampTwo));
		CHECK(CountInk(Test, Field) > ShortInk);
	}

	TEST_CASE("A reference to an asset is the path that is typed, and an empty field is none")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FScopedTempDirectory Directory;
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Directory", FFileHelper::PathToUtf8(Directory.GetPath() / "Project"));
		Arguments.SetMember("Name", "Game");
		REQUIRE(Test.Editor.ExecuteCommand("Project.New", Arguments).HasSucceeded());
		std::string Error;
		REQUIRE(FImageFile::SavePng(
			FImage(2, 2, FColor(255, 0, 0, 255)), Directory.GetPath() / "Project" / "Content" / "Wood.png", Error));
		const FGadgetIds Ids = AddSelectedGadget(Test);

		Test.Click(Row(Ids.Component, "Picture"));
		Test.Type("Wood.png");
		Test.PressKey(EKey::Enter);

		REQUIRE(GetGadget(Test, Ids).Picture != nullptr);
		CHECK(GetGadget(Test, Ids).Picture->GetAssetPath() == "Wood.png");
		CHECK(GetLastStepName(Test) == "Set Picture");

		// A path that leads to nothing leaves the reference as it is, and the editor's log says why.
		Test.Click(Row(Ids.Component, "Picture"));
		Test.Type("Stone.png");
		Test.PressKey(EKey::Enter);
		REQUIRE(GetGadget(Test, Ids).Picture != nullptr);
		CHECK(GetGadget(Test, Ids).Picture->GetAssetPath() == "Wood.png");

		// Emptied, the field refers to no asset.
		Test.Click(Row(Ids.Component, "Picture"));
		Test.PressKey(EKey::Delete);
		Test.PressKey(EKey::Enter);
		CHECK(GetGadget(Test, Ids).Picture == nullptr);
	}

	TEST_CASE("A list grows by the button beside it, and loses the element whose cross is clicked")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);
		REQUIRE(GetGadget(Test, Ids).Tags.empty());
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Tags/0")));

		Test.Click(Row(Ids.Component, "Tags/Add"));
		REQUIRE(GetGadget(Test, Ids).Tags.size() == 1);
		CHECK(GetLastStepName(Test) == "Set Tags");

		Test.Click(Row(Ids.Component, "Tags/0"));
		Test.Type("red");
		Test.PressKey(EKey::Enter);
		Test.Click(Row(Ids.Component, "Tags/Add"));
		Test.Click(Row(Ids.Component, "Tags/1"));
		Test.Type("hot");
		Test.PressKey(EKey::Enter);
		const std::vector<std::string> Both = {"red", "hot"};
		CHECK(GetGadget(Test, Ids).Tags == Both);

		Test.Click(Row(Ids.Component, "Tags/0/Remove"));
		CHECK(GetGadget(Test, Ids).Tags == std::vector<std::string>{"hot"});
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Tags/1")));

		// The arrow of the list puts back the list of a new component, which is empty.
		Test.Click(Row(Ids.Component, "Tags/Reset"));
		CHECK(GetGadget(Test, Ids).Tags.empty());
		CHECK(GetLastStepName(Test) == "Reset Tags");
	}

	TEST_CASE("A new element of a list of numbers is zero, and is typed like any number")
	{
		FEditorUiTest Test;
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Board");
		const int64 Component = Entity->AddComponent<WEditorTestComponent>()->GetId();
		Test.Editor.SetSelection({Entity->GetId()});
		Test.DrawFrame();
		const auto GetScores = [&Test, Component]()
		{
			return Cast<WEditorTestComponent>(Test.Editor.GetScene().FindComponentById(Component))->Scores;
		};

		Test.Click(Row(Component, "Scores/Add"));
		CHECK(GetScores() == std::vector<int32>{0});

		TypeNumber(Test, Row(Component, "Scores/0"), "7");
		Test.Click(Row(Component, "Scores/Add"));

		const std::vector<int32> Expected = {7, 0};
		CHECK(GetScores() == Expected);
		CHECK(GetLastStepName(Test) == "Set Scores");
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}
	TEST_CASE("Properties are under the categories they were declared with, which can be closed")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);
		const std::string Motion = std::format("Details/{}/Category/Motion", Ids.Component);

		// Without a category, a property is under the heading of the component itself.
		CHECK(Test.HasItem(Row(Ids.Component, "bEnabled")));
		REQUIRE(Test.HasItem(Motion));
		CHECK(Test.HasItem(std::format("Details/{}/Category/Look", Ids.Component)));
		CHECK(Test.HasItem(std::format("Details/{}/Category/Links", Ids.Component)));
		CHECK(Test.HasItem(Row(Ids.Component, "Speed")));

		Test.Click(Motion);
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Speed")));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Offset/X")));
		CHECK(Test.HasItem(Row(Ids.Component, "Tint")));
		CHECK(Test.HasItem(Row(Ids.Component, "bEnabled")));

		// The heading of the component closes all of it.
		Test.Click(std::format("Details/{}", Ids.Component));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "bEnabled")));
		CHECK_FALSE(Test.HasItem(Motion));
		CHECK(Test.HasItem(Row(Ids.Entity, "RelativeTransform/Location/X")));
	}

	TEST_CASE("Only what contains the text of the search field is shown")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		const FGadgetIds Ids = AddSelectedGadget(Test);
		const auto Search = [&Test](std::string_view Text)
		{
			Test.Click("Details/Search");
			Test.PressKey(EKey::A, {EKey::LeftControl});
			Test.PressKey(EKey::Delete);
			Test.Type(Text);
		};

		// The name of a property, whatever its capitals.
		Search("spe");
		CHECK(Test.HasItem(Row(Ids.Component, "Speed")));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Offset/X")));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "bEnabled")));
		CHECK_FALSE(Test.HasItem("Details/Transform"));

		// A category brings all its properties.
		Search("LOOK");
		CHECK(Test.HasItem(Row(Ids.Component, "Tint")));
		CHECK(Test.HasItem(Row(Ids.Component, "Picture")));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Speed")));

		// A part of the entity's transform is found by its name, and all three by the heading.
		Search("scale");
		CHECK(Test.HasItem(Row(Ids.Entity, "RelativeTransform/Scale/X")));
		CHECK_FALSE(Test.HasItem(Row(Ids.Entity, "RelativeTransform/Location/X")));
		Search("transform");
		CHECK(Test.HasItem(Row(Ids.Entity, "RelativeTransform/Location/X")));
		CHECK(Test.HasItem(Row(Ids.Entity, "RelativeTransform/Scale/X")));
		CHECK_FALSE(Test.HasItem(Row(Ids.Component, "Speed")));

		// The name of a component brings everything it has.
		Search("gadget");
		CHECK(Test.HasItem(Row(Ids.Component, "bEnabled")));
		CHECK(Test.HasItem(Row(Ids.Component, "Speed")));
		CHECK_FALSE(Test.HasItem("Details/Transform"));

		Search("no such thing");
		CHECK_FALSE(Test.HasItem(std::format("Details/{}", Ids.Component)));
		CHECK_FALSE(Test.HasItem("Details/Transform"));
		CHECK(Test.HasItem("Details/Name"));

		Search("");
		CHECK(Test.HasItem("Details/Transform"));
		CHECK(Test.HasItem(Row(Ids.Component, "Speed")));
	}

	TEST_CASE("Add Component lists the classes that can be added, and adds the one that is clicked")
	{
		FEditorUiTest Test;
		const int64 Lamp = AddSelectedEntity(Test, "Lamp");
		CHECK_FALSE(Test.HasItem("Details/AddComponent/WPointLightComponent"));

		Test.Click("Details/AddComponent");
		CHECK(Test.HasItem("Details/AddComponent/WPointLightComponent"));
		CHECK(Test.HasItem("Details/AddComponent/WDirectionalLightComponent"));
		CHECK_FALSE(Test.HasItem("Details/AddComponent/WComponent"));
		CHECK_FALSE(Test.HasItem("Details/AddComponent/WMissingComponent"));

		// The list opens with its search field ready: what is typed leaves the classes whose names contain it.
		Test.Type("point");
		CHECK(Test.HasItem("Details/AddComponent/WPointLightComponent"));
		CHECK_FALSE(Test.HasItem("Details/AddComponent/WDirectionalLightComponent"));

		Test.Click("Details/AddComponent/WPointLightComponent");

		const WEntity& Entity = GetEntity(Test, Lamp);
		REQUIRE(Entity.GetComponents().size() == 1);
		const WComponent* const Light = Entity.GetComponents()[0];
		CHECK(Light->IsA<WPointLightComponent>());
		CHECK(GetLastStepName(Test) == "Add Point Light Component");
		CHECK_FALSE(Test.HasItem("Details/AddComponent/WPointLightComponent"));
		// The new component is shown with its properties.
		CHECK(Test.HasItem(Row(Light->GetId(), "Intensity")));

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Lamp).GetComponents().empty());
	}

	TEST_CASE("The menu of a component removes it")
	{
		FEditorUiTest Test;
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Lamp");
		const int64 Lamp = Entity->GetId();
		const int64 Light = Entity->AddComponent<WPointLightComponent>()->GetId();
		const int64 Label = Entity->AddComponent<WEditorTestComponent>()->GetId();
		Test.Editor.SetSelection({Lamp});
		Test.DrawFrame();
		CHECK_FALSE(Test.HasItem("Details/Menu/Remove Component"));

		SUBCASE("Opened with the right mouse button on the heading")
		{
			Test.Click(std::format("Details/{}", Light), EMouseButton::Right);
		}
		SUBCASE("Opened with the button at the end of the heading")
		{
			Test.Click(std::format("Details/{}/Menu", Light));
		}
		REQUIRE(Test.HasItem("Details/Menu/Remove Component"));
		Test.Click("Details/Menu/Remove Component");

		const WEntity& After = GetEntity(Test, Lamp);
		REQUIRE(After.GetComponents().size() == 1);
		CHECK(After.GetComponents()[0]->GetId() == Label);
		CHECK(GetLastStepName(Test) == "Remove Point Light Component");
		CHECK_FALSE(Test.HasItem(std::format("Details/{}", Light)));

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Lamp).GetComponents().size() == 2);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A component whose class is not in this build is shown, and can be removed")
	{
		FEditorUiTest Test;
		{
			// A scene as another build saved it, with a component of a class that this build does not have.
			WScene* const Original = NewObject<WScene>();
			WEntity* const Entity = Original->CreateEntity("Crate");
			REQUIRE(Entity->AddComponent<WEditorTestComponent>() != nullptr);
			std::string Text = FSceneSerializer::SaveToText(*Original);
			Original->Destroy();
			const std::string KnownClass = "WEditorTestComponent";
			const std::size_t Position = Text.find(KnownClass);
			REQUIRE(Position != std::string::npos);
			Text.replace(Position, KnownClass.size(), "WPluginRigidBodyComponent");

			std::string Error;
			WScene* const Loaded = FSceneSerializer::LoadFromText(Text, Error);
			REQUIRE(Loaded != nullptr);
			Test.Editor.SetScene(*Loaded, std::filesystem::path());
		}
		const WEntity* const Crate = Test.Editor.GetScene().FindEntityByName("Crate");
		REQUIRE(Crate != nullptr);
		const int64 CrateId = Crate->GetId();
		REQUIRE(Crate->GetComponents().size() == 1);
		const int64 Missing = Crate->GetComponents()[0]->GetId();
		REQUIRE(Crate->GetComponents()[0]->IsA<WMissingComponent>());
		Test.Editor.SetSelection({CrateId});
		Test.DrawFrame();

		// It has a heading and nothing under it that could be changed.
		CHECK(Test.HasItem(std::format("Details/{}", Missing)));
		CHECK_FALSE(Test.HasItem(Row(Missing, "Health")));

		Test.Click(std::format("Details/{}/Menu", Missing));
		Test.Click("Details/Menu/Remove Component");

		CHECK(GetEntity(Test, CrateId).GetComponents().empty());
		CHECK(GetLastStepName(Test) == "Remove Plugin Rigid Body Component");
	}

	TEST_CASE("The heading of a component whose class is missing says all of what the file calls the class")
	{
		FEditorUiTest Test;
		{
			// A scene with two components of classes that no build has. The name of the second has in it what
			// the library that lays the panel out takes for the end of a text.
			WScene* const Original = NewObject<WScene>();
			WEntity* const Entity = Original->CreateEntity("Crate");
			REQUIRE(Entity->AddComponent<WEditorTestComponent>() != nullptr);
			REQUIRE(Entity->AddComponent<WEditorTestComponent>() != nullptr);
			std::string Text = FSceneSerializer::SaveToText(*Original);
			Original->Destroy();
			const std::string KnownClass = "WEditorTestComponent";
			const std::size_t First = Text.find(KnownClass);
			REQUIRE(First != std::string::npos);
			Text.replace(First, KnownClass.size(), "WGone");
			const std::size_t Second = Text.find(KnownClass);
			REQUIRE(Second != std::string::npos);
			Text.replace(Second, KnownClass.size(), "WGone##AndMuchMoreThanThat");

			std::string Error;
			WScene* const Loaded = FSceneSerializer::LoadFromText(Text, Error);
			REQUIRE(Loaded != nullptr);
			Test.Editor.SetScene(*Loaded, std::filesystem::path());
		}
		const WEntity* const Crate = Test.Editor.GetScene().FindEntityByName("Crate");
		REQUIRE(Crate != nullptr);
		REQUIRE(Crate->GetComponents().size() == 2);
		const int64 ShortId = Crate->GetComponents()[0]->GetId();
		const int64 LongId = Crate->GetComponents()[1]->GetId();
		Test.Editor.SetSelection({Crate->GetId()});
		Test.DrawFrames(3);

		// The longer name is written with more ink, which it would not be if it ended after the first word.
		CHECK(CountInk(Test, std::format("Details/{}", LongId)) > CountInk(Test, std::format("Details/{}", ShortId)));

		// The heading still closes and opens what is below it, and still has its menu.
		Test.Click(std::format("Details/{}/Menu", LongId));
		Test.Click("Details/Menu/Remove Component");
		CHECK(GetEntity(Test, Crate->GetId()).GetComponents().size() == 1);
	}

	TEST_CASE("The step that a dragged number began ends when the number is no longer shown")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");
		const FVector2D Field = Test.RequireItem(Row(Player, "RelativeTransform/Location/X")).GetCenter();
		Test.MoveMouse(Field);
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Test.DrawFrame();
		Test.MoveMouse(Field + FVector2D(20.0, 0.0));
		Test.MoveMouse(Field + FVector2D(40.0, 0.0));
		const double Dragged = GetEntity(Test, Player).GetRelativeTransform().Location.x;
		REQUIRE(Dragged > 0.0);
		REQUIRE(Test.Editor.IsStepOpen());

		// With the mouse button still down, something else selects nothing: an agent, say. The number is gone
		// from the panel, and nobody will ever let go of it.
		Test.Editor.SetSelection({});
		Test.DrawFrame();

		CHECK(Test.HasItem("Details/NothingSelected"));
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == Dragged);
		CHECK(GetLastStepName(Test) == "Set Transform");

		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Released);
		Test.DrawFrame();
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == Dragged);
	}

	TEST_CASE("Something else that the user does while a number is dragged is a step of its own")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");
		const FVector2D Field = Test.RequireItem(Row(Player, "RelativeTransform/Location/X")).GetCenter();
		Test.MoveMouse(Field);
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Test.DrawFrame();
		Test.MoveMouse(Field + FVector2D(20.0, 0.0));
		Test.MoveMouse(Field + FVector2D(40.0, 0.0));
		const double Dragged = GetEntity(Test, Player).GetRelativeTransform().Location.x;
		REQUIRE(Dragged > 0.0);
		REQUIRE(Test.Editor.IsStepOpen());

		// The other hand deletes the entity, with the mouse button still down.
		Test.PressKey(EKey::Delete);

		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK_FALSE(Test.Editor.IsStepOpen());
		const std::vector<std::string> Steps = Test.Editor.GetHistory().GetUndoStepNames();
		REQUIRE(Steps.size() >= 2);
		CHECK(Steps[0] == "Delete");
		CHECK(Steps[1] == "Set Transform");
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Released);
		Test.DrawFrame();

		// One undo brings the entity back where it was dragged to, and another puts it where it was before.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == Dragged);
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == 0.0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}
	TEST_CASE("An undo while a number is dragged takes back the drag so far, and what follows is another step")
	{
		FEditorUiTest Test;
		const int64 Player = AddSelectedEntity(Test, "Player");
		const FVector2D Field = Test.RequireItem(Row(Player, "RelativeTransform/Location/X")).GetCenter();
		Test.MoveMouse(Field);
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Test.DrawFrame();
		Test.MoveMouse(Field + FVector2D(20.0, 0.0));
		Test.MoveMouse(Field + FVector2D(40.0, 0.0));
		REQUIRE(GetEntity(Test, Player).GetRelativeTransform().Location.x > 0.0);
		REQUIRE(Test.Editor.IsStepOpen());

		// As an agent would give it: the keyboard is not what this is about.
		REQUIRE(Test.Editor.ExecuteCommand("Edit.Undo", FJsonValue()).HasSucceeded());
		Test.DrawFrame();
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == 0.0);
		CHECK_FALSE(Test.Editor.IsStepOpen());

		// The hand moves on, and the number with it, from where the undo put it.
		Test.MoveMouse(Field + FVector2D(60.0, 0.0));
		const double DraggedOn = GetEntity(Test, Player).GetRelativeTransform().Location.x;
		CHECK(DraggedOn > 0.0);
		CHECK(Test.Editor.IsStepOpen());
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Released);
		Test.DrawFrames(2);

		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == DraggedOn);
		CHECK(GetLastStepName(Test) == "Set Transform");
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetEntity(Test, Player).GetRelativeTransform().Location.x == 0.0);
	}
}
