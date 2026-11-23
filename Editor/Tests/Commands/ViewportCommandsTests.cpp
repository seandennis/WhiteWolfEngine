#include "Editor.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <format>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** How far from a point the camera's focus point may be, in metres, and still count as being on it. */
	constexpr double FocusTolerance = 1.0e-4;
}

TEST_SUITE("Viewport commands")
{
	TEST_CASE("A new editor looks at its scene with a valid camera, in the lit view mode")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		CHECK(Editor.GetViewportCamera() == FViewportCamera());
		CHECK(Editor.GetViewportCamera().IsValid());
		CHECK(Editor.GetViewMode() == ESceneViewMode::Lit);
	}

	TEST_CASE("Viewport.GetCamera returns where the camera is and which way it faces")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Viewport.GetCamera", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"Location":[4.25,3.0,6.0],"Yaw":35.0,"Pitch":-22.0,"FieldOfView":60.0,"Exposure":1.0})");
	}

	TEST_CASE("Viewport.SetCamera moves the camera")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Viewport.SetCamera",
			ParseTestJson(
				R"({"Location": [1, 2.5, -3], "Yaw": 90, "Pitch": -45.5, "FieldOfView": 75, "Exposure": 0.25})"));

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		const FViewportCamera& Camera = Editor.GetViewportCamera();
		CHECK(Camera.Location == FVector(1.0, 2.5, -3.0));
		CHECK(Camera.Yaw == 90.0f);
		CHECK(Camera.Pitch == -45.5f);
		CHECK(Camera.VerticalFieldOfView == 75.0f);
		CHECK(Camera.Exposure == 0.25f);

		const FCommandResult Read = Editor.ExecuteCommand("Viewport.GetCamera", FJsonValue());
		REQUIRE(Read.HasSucceeded());
		CHECK(WriteTestJson(Read.GetValue()) ==
			R"({"Location":[1.0,2.5,-3.0],"Yaw":90.0,"Pitch":-45.5,"FieldOfView":75.0,"Exposure":0.25})");
	}

	TEST_CASE("Viewport.SetCamera leaves what it is not given as it is")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FViewportCamera Before = Editor.GetViewportCamera();

		REQUIRE(Editor.ExecuteCommand("Viewport.SetCamera", ParseTestJson(R"({"Yaw": -120})")).HasSucceeded());

		FViewportCamera Expected = Before;
		Expected.Yaw = -120.0f;
		CHECK(Editor.GetViewportCamera() == Expected);

		REQUIRE(Editor.ExecuteCommand("Viewport.SetCamera", FJsonValue()).HasSucceeded());
		CHECK(Editor.GetViewportCamera() == Expected);
	}

	TEST_CASE("Viewport.SetCamera with LookAt turns the camera to face a point")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		SUBCASE("From where it is")
		{
			REQUIRE(Editor.ExecuteCommand("Viewport.SetCamera", ParseTestJson(R"({"Location": [0, 0, 10]})"))
						.HasSucceeded());

			REQUIRE(
				Editor.ExecuteCommand("Viewport.SetCamera", ParseTestJson(R"({"LookAt": [3, 4, 10]})")).HasSucceeded());
		}
		SUBCASE("From the location given with it")
		{
			REQUIRE(Editor
						.ExecuteCommand(
							"Viewport.SetCamera", ParseTestJson(R"({"LookAt": [3, 4, 10], "Location": [0, 0, 10]})"))
						.HasSucceeded());
		}

		const FViewportCamera& Camera = Editor.GetViewportCamera();
		CHECK(Camera.Location == FVector(0.0, 0.0, 10.0));
		CHECK(glm::length(Camera.GetForward() - FVector3f(0.6f, 0.8f, 0.0f)) < 1.0e-5f);
	}

	TEST_CASE("Viewport.SetCamera refuses numbers that no picture can be drawn from, and leaves the camera")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FViewportCamera Before = Editor.GetViewportCamera();
		std::string Arguments;
		std::string ExpectedError;

		SUBCASE("A location that is not three numbers")
		{
			Arguments = R"({"Location": [1, 2]})";
			ExpectedError = "Location must be a position: three finite numbers, as [x, y, z].";
		}
		SUBCASE("A location that is not an array")
		{
			Arguments = R"({"Location": "origin"})";
			ExpectedError = "Location must be a position: three finite numbers, as [x, y, z].";
		}
		SUBCASE("A pitch beyond straight up")
		{
			Arguments = R"({"Pitch": 90})";
			ExpectedError = "Pitch must be between -89 and 89 degrees, and 90 was given.";
		}
		SUBCASE("A pitch beyond straight down")
		{
			Arguments = R"({"Yaw": 10, "Pitch": -120.5})";
			ExpectedError = "Pitch must be between -89 and 89 degrees, and -120.5 was given.";
		}
		SUBCASE("A yaw too large to be a number")
		{
			Arguments = R"({"Yaw": 1e300})";
			ExpectedError = "Yaw must be a finite number.";
		}
		SUBCASE("A field of view of nothing")
		{
			Arguments = R"({"FieldOfView": 0})";
			ExpectedError = "FieldOfView must be between 1 and 179 degrees, and 0 was given.";
		}
		SUBCASE("A field of view of half a turn")
		{
			Arguments = R"({"FieldOfView": 180})";
			ExpectedError = "FieldOfView must be between 1 and 179 degrees, and 180 was given.";
		}
		SUBCASE("No exposure")
		{
			Arguments = R"({"Exposure": 0})";
			ExpectedError = "Exposure must be greater than zero, and 0 was given.";
		}
		SUBCASE("A negative exposure, with a location that is fine")
		{
			Arguments = R"({"Location": [9, 9, 9], "Exposure": -2})";
			ExpectedError = "Exposure must be greater than zero, and -2 was given.";
		}
		SUBCASE("A point to look at together with a yaw")
		{
			Arguments = R"({"LookAt": [0, 0, 0], "Yaw": 10})";
			ExpectedError = "LookAt says which way the camera faces, so Yaw and Pitch cannot be given with it.";
		}
		SUBCASE("A point to look at together with a pitch")
		{
			Arguments = R"({"LookAt": [0, 0, 0], "Pitch": 10})";
			ExpectedError = "LookAt says which way the camera faces, so Yaw and Pitch cannot be given with it.";
		}
		SUBCASE("A point to look at that is where the camera is")
		{
			Arguments = R"({"LookAt": [4.25, 3, 6]})";
			ExpectedError = "The camera cannot look at the point it is at.";
		}
		SUBCASE("A point to look at that is where the camera is moved to")
		{
			Arguments = R"({"Location": [1, 1, 1], "LookAt": [1, 1, 1]})";
			ExpectedError = "The camera cannot look at the point it is at.";
		}
		SUBCASE("A point to look at that is not a position")
		{
			Arguments = R"({"LookAt": 5})";
			ExpectedError = "LookAt must be a position: three finite numbers, as [x, y, z].";
		}

		const FCommandResult Result = Editor.ExecuteCommand("Viewport.SetCamera", ParseTestJson(Arguments));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Editor.GetViewportCamera() == Before);
	}

	TEST_CASE("Viewport.SetCamera refuses numbers that are not finite, which only code can give it")
	{
		// JSON text has no way to write infinity, but arguments that are put together in code can hold it.
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FViewportCamera Before = Editor.GetViewportCamera();
		const double Infinity = std::numeric_limits<double>::infinity();
		FJsonValue Arguments = FJsonValue::MakeObject();
		std::string ExpectedError;

		SUBCASE("A location at infinity")
		{
			Arguments.SetMember(
				"Location", FJsonValue(FJsonValue::FArray{FJsonValue(0.0), FJsonValue(Infinity), FJsonValue(0.0)}));
			ExpectedError = "Location must be a position: three finite numbers, as [x, y, z].";
		}
		SUBCASE("A point to look at that is not a number")
		{
			const double NotANumber = std::numeric_limits<double>::quiet_NaN();
			Arguments.SetMember(
				"LookAt", FJsonValue(FJsonValue::FArray{FJsonValue(NotANumber), FJsonValue(0.0), FJsonValue(0.0)}));
			ExpectedError = "LookAt must be a position: three finite numbers, as [x, y, z].";
		}
		SUBCASE("An infinite exposure")
		{
			Arguments.SetMember("Exposure", FJsonValue(Infinity));
			ExpectedError = "Exposure must be a finite number.";
		}
		SUBCASE("A pitch of minus infinity")
		{
			Arguments.SetMember("Pitch", FJsonValue(-Infinity));
			ExpectedError = "Pitch must be a finite number.";
		}

		const FCommandResult Result = Editor.ExecuteCommand("Viewport.SetCamera", Arguments);

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Editor.GetViewportCamera() == Before);
	}

	TEST_CASE("The view mode is read and changed by name")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Initial = Editor.ExecuteCommand("Viewport.GetViewMode", FJsonValue());
		REQUIRE(Initial.HasSucceeded());
		CHECK(WriteTestJson(Initial.GetValue()) == R"({"Mode":"Lit"})");

		std::string Name;
		ESceneViewMode ExpectedMode = ESceneViewMode::Lit;
		SUBCASE("Unlit")
		{
			Name = "Unlit";
			ExpectedMode = ESceneViewMode::Unlit;
		}
		SUBCASE("Ambient occlusion")
		{
			Name = "AmbientOcclusion";
			ExpectedMode = ESceneViewMode::AmbientOcclusion;
		}
		SUBCASE("Lit, as it is already")
		{
			Name = "Lit";
			ExpectedMode = ESceneViewMode::Lit;
		}

		const FCommandResult Result =
			Editor.ExecuteCommand("Viewport.SetViewMode", ParseTestJson(R"({"Mode": ")" + Name + R"("})"));

		REQUIRE(Result.HasSucceeded());
		CHECK(Editor.GetViewMode() == ExpectedMode);
		const FCommandResult Read = Editor.ExecuteCommand("Viewport.GetViewMode", FJsonValue());
		REQUIRE(Read.HasSucceeded());
		CHECK(WriteTestJson(Read.GetValue()) == R"({"Mode":")" + Name + R"("})");
	}

	TEST_CASE("Viewport.SetViewMode refuses a view mode that there is not, and says which there are")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.SetViewMode(ESceneViewMode::Unlit);

		const FCommandResult Result =
			Editor.ExecuteCommand("Viewport.SetViewMode", ParseTestJson(R"({"Mode": "Wireframe"})"));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			"There is no view mode called 'Wireframe'. The view modes are Lit, Unlit and AmbientOcclusion.");
		CHECK(Editor.GetViewMode() == ESceneViewMode::Unlit);
	}

	TEST_CASE("Moving the camera and changing the view mode are not steps of the history, and a new scene keeps them")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());

		REQUIRE(
			Editor.ExecuteCommand("Viewport.SetCamera", ParseTestJson(R"({"Location": [0, 50, 0]})")).HasSucceeded());
		REQUIRE(Editor.ExecuteCommand("Viewport.SetViewMode", ParseTestJson(R"({"Mode": "Unlit"})")).HasSucceeded());

		const std::vector<std::string> ExpectedSteps = {"Entity.Create"};
		CHECK(Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);

		REQUIRE(Editor.Undo().has_value());
		REQUIRE(Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());

		CHECK(Editor.GetViewportCamera().Location == FVector(0.0, 50.0, 0.0));
		CHECK(Editor.GetViewMode() == ESceneViewMode::Unlit);
	}

	TEST_CASE("Giving the editor a camera that no picture can be drawn from is a fatal error")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		const FViewportCamera Before = Editor.GetViewportCamera();
		FViewportCamera Broken;
		Broken.Exposure = std::numeric_limits<float>::quiet_NaN();

		CHECK_THROWS_AS(Editor.SetViewportCamera(Broken), FFatalErrorException);

		CHECK(Editor.GetViewportCamera() == Before);
	}

	TEST_CASE("Viewport.Focus brings the camera to where it sees what is selected, without turning it")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Crate = Editor.GetScene().CreateEntity("Crate");
		Crate->SetWorldLocation(FVector(10.0, 0.0, -4.0));
		Crate->AddComponent<WStaticMeshComponent>()->Mesh = FMeshShapes::CreateBox(FVector3f(2.0f));
		Editor.SetSelection({Crate->GetId()});
		const FViewportCamera Before = Editor.GetViewportCamera();

		const FCommandResult Result = Editor.ExecuteCommand("Viewport.Focus", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		const FViewportCamera& After = Editor.GetViewportCamera();
		CHECK(glm::length(After.GetFocusPoint() - FVector(10.0, 0.0, -4.0)) < FocusTolerance);
		CHECK(After.Yaw == Before.Yaw);
		CHECK(After.Pitch == Before.Pitch);

		// The corners of a box of two metres are the square root of three from its middle. A ball of that radius
		// fills a picture of 60 degrees from twice as far, and the camera stands back a little more.
		CHECK(After.FocusDistance == doctest::Approx(std::sqrt(3.0) * 2.0 * FViewportCamera::FrameMargin));
	}

	TEST_CASE("Viewport.Focus looks at the entities it is given, with the meshes of what is below them")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WScene& Scene = Editor.GetScene();
		// A cart that shows nothing itself, far from its wheel, which does.
		WEntity* const Cart = Scene.CreateEntity("Cart");
		WEntity* const Wheel = Scene.CreateEntity("Wheel", Cart);
		Wheel->SetWorldLocation(FVector(100.0, 0.0, 0.0));
		Wheel->AddComponent<WStaticMeshComponent>()->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		WEntity* const Crate = Scene.CreateEntity("Crate");
		Crate->SetWorldLocation(FVector(110.0, 0.0, 0.0));
		Crate->AddComponent<WStaticMeshComponent>()->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		const WEntity* const Other = Scene.CreateEntity("Other");
		Editor.SetSelection({Other->GetId()});

		REQUIRE(
			Editor
				.ExecuteCommand("Viewport.Focus", ParseTestJson(std::format(R"({{"Entities": [{}]}})", Cart->GetId())))
				.HasSucceeded());

		// What is seen of the cart is its wheel: that is looked at, not the place of the cart.
		CHECK(glm::length(Editor.GetViewportCamera().GetFocusPoint() - FVector(100.0, 0.0, 0.0)) < FocusTolerance);
		CHECK(Editor.GetSelection() == std::vector<int64>{Other->GetId()});

		// Several entities are looked at together: the middle of the room they take up, from where all of it shows.
		REQUIRE(Editor
					.ExecuteCommand("Viewport.Focus",
						ParseTestJson(std::format(R"({{"Entities": [{}, {}]}})", Cart->GetId(), Crate->GetId())))
					.HasSucceeded());
		const FViewportCamera& Camera = Editor.GetViewportCamera();
		CHECK(glm::length(Camera.GetFocusPoint() - FVector(105.0, 0.0, 0.0)) < FocusTolerance);
		const double Radius = 0.5 * std::sqrt(11.0 * 11.0 + 1.0 + 1.0);
		CHECK(Camera.FocusDistance == doctest::Approx(Radius * 2.0 * FViewportCamera::FrameMargin));
	}

	TEST_CASE("Viewport.Focus looks at entities that show nothing where they are, from about an arm's length")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Lamp = Editor.GetScene().CreateEntity("Lamp");
		Lamp->SetWorldLocation(FVector(3.0, 4.0, 5.0));
		Editor.SetSelection({Lamp->GetId()});

		REQUIRE(Editor.ExecuteCommand("Viewport.Focus", FJsonValue()).HasSucceeded());

		const FViewportCamera& Camera = Editor.GetViewportCamera();
		CHECK(glm::length(Camera.GetFocusPoint() - FVector(3.0, 4.0, 5.0)) < FocusTolerance);
		CHECK(Camera.FocusDistance == doctest::Approx(0.5 * 2.0 * FViewportCamera::FrameMargin));
	}

	TEST_CASE("Viewport.Focus fails, and leaves the camera where it is, when there is nothing to look at")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Crate = Editor.GetScene().CreateEntity("Crate");
		const FViewportCamera Before = Editor.GetViewportCamera();
		std::string ArgumentText = "null";
		std::string ExpectedError;

		SUBCASE("Nothing is selected and nothing is given")
		{
			ExpectedError = "No entity is selected and none was given: there is nothing to look at.";
		}
		SUBCASE("No entities are given")
		{
			Editor.SetSelection({Crate->GetId()});
			ArgumentText = R"({"Entities": []})";
			ExpectedError = "Entities is empty: there is nothing to look at.";
		}
		SUBCASE("Entities is not a list")
		{
			ArgumentText = std::format(R"({{"Entities": {}}})", Crate->GetId());
			ExpectedError = "Entities must be an array of entity identifiers, but is a number.";
		}
		SUBCASE("An entity that does not exist")
		{
			ArgumentText = std::format(R"({{"Entities": [{}, 99]}})", Crate->GetId());
			ExpectedError = "There is no entity with the identifier 99.";
		}
		SUBCASE("An entity that is nowhere a camera can look")
		{
			Crate->SetWorldLocation(FVector(std::numeric_limits<double>::infinity(), 0.0, 0.0));
			Editor.SetSelection({Crate->GetId()});
			ExpectedError = "The entities are not at places that a camera can look at: their positions are not "
							"finite numbers.";
		}

		const FCommandResult Result = Editor.ExecuteCommand("Viewport.Focus", ParseTestJson(ArgumentText));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Editor.GetViewportCamera() == Before);
	}
}
