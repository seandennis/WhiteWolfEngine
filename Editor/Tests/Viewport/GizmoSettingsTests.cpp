#include "Viewport/GizmoSettings.h"

#include "Editor.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <limits>

TEST_SUITE("FGizmoSettings")
{
	TEST_CASE("New settings move things along the axes of the world, without steps")
	{
		const FGizmoSettings Settings;

		CHECK(Settings.IsValid());
		CHECK(Settings.Mode == EGizmoMode::Translate);
		CHECK(Settings.Space == EGizmoSpace::World);
		CHECK_FALSE(Settings.bSnapLocation);
		CHECK_FALSE(Settings.bSnapRotation);
		CHECK_FALSE(Settings.bSnapScale);
		// The steps that are used once they are switched on: ten centimetres, ten degrees and a quarter.
		CHECK(Settings.LocationSnap == 0.1);
		CHECK(Settings.RotationSnap == 10.0f);
		CHECK(Settings.ScaleSnap == 0.25f);
	}

	TEST_CASE("The modes follow one another in turn: moving, turning, sizing, and moving again")
	{
		CHECK(FGizmoSettings::GetNextMode(EGizmoMode::Translate) == EGizmoMode::Rotate);
		CHECK(FGizmoSettings::GetNextMode(EGizmoMode::Rotate) == EGizmoMode::Scale);
		CHECK(FGizmoSettings::GetNextMode(EGizmoMode::Scale) == EGizmoMode::Translate);
	}

	TEST_CASE("Settings are valid while every step is a number within its limits")
	{
		FGizmoSettings Settings;
		Settings.Mode = EGizmoMode::Scale;
		Settings.Space = EGizmoSpace::Local;
		Settings.bSnapLocation = true;
		Settings.LocationSnap = FGizmoSettings::MinLocationSnap;
		Settings.RotationSnap = FGizmoSettings::MaxRotationSnap;
		Settings.ScaleSnap = FGizmoSettings::MinScaleSnap;
		CHECK(Settings.IsValid());

		Settings.LocationSnap = FGizmoSettings::MaxLocationSnap;
		Settings.RotationSnap = FGizmoSettings::MinRotationSnap;
		Settings.ScaleSnap = FGizmoSettings::MaxScaleSnap;
		CHECK(Settings.IsValid());
	}

	TEST_CASE("Settings with a step that is no number, nothing, or out of range are not valid")
	{
		const double NoNumber = std::numeric_limits<double>::quiet_NaN();
		const float NoSmallNumber = std::numeric_limits<float>::quiet_NaN();

		for (const double Step : {0.0,
				 -0.1,
				 FGizmoSettings::MinLocationSnap * 0.5,
				 FGizmoSettings::MaxLocationSnap * 2.0,
				 NoNumber,
				 std::numeric_limits<double>::infinity()})
		{
			FGizmoSettings Settings;
			Settings.LocationSnap = Step;
			CHECK_FALSE(Settings.IsValid());
		}

		for (const float Step : {0.0f, -10.0f, 0.01f, 181.0f, NoSmallNumber})
		{
			FGizmoSettings Settings;
			Settings.RotationSnap = Step;
			CHECK_FALSE(Settings.IsValid());
		}

		for (const float Step : {0.0f, -0.25f, 0.0001f, 11.0f, NoSmallNumber})
		{
			FGizmoSettings Settings;
			Settings.ScaleSnap = Step;
			CHECK_FALSE(Settings.IsValid());
		}
	}

	TEST_CASE("Settings are equal when all that they say is")
	{
		const FGizmoSettings Settings;
		FGizmoSettings Other;
		CHECK(Other == Settings);

		Other.Mode = EGizmoMode::Rotate;
		CHECK_FALSE(Other == Settings);

		Other = Settings;
		Other.bSnapRotation = true;
		CHECK_FALSE(Other == Settings);

		Other = Settings;
		Other.ScaleSnap = 0.5f;
		CHECK_FALSE(Other == Settings);
	}
}

TEST_SUITE("FEditor gizmo settings")
{
	TEST_CASE("The editor keeps the gizmo's settings, which are not part of the scene")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		CHECK(Editor.GetGizmoSettings() == FGizmoSettings());

		FGizmoSettings Settings;
		Settings.Mode = EGizmoMode::Rotate;
		Settings.Space = EGizmoSpace::Local;
		Settings.bSnapRotation = true;
		Settings.RotationSnap = 15.0f;
		Editor.SetGizmoSettings(Settings);

		CHECK(Editor.GetGizmoSettings() == Settings);
		// Setting the gizmo changes nothing that could be undone.
		CHECK_FALSE(Editor.GetHistory().CanUndo());

		// Another scene is worked on with the gizmo as it was.
		REQUIRE(Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());
		CHECK(Editor.GetGizmoSettings() == Settings);
	}

	TEST_CASE("Settings that are not valid are a fatal error, and leave the gizmo as it was")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		FGizmoSettings Broken;
		Broken.LocationSnap = 0.0;

		CHECK_THROWS_AS(Editor.SetGizmoSettings(Broken), FFatalErrorException);

		CHECK(Editor.GetGizmoSettings() == FGizmoSettings());
	}
}
