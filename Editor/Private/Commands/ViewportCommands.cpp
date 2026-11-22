#include "Commands/ViewportCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Reflection/PropertyValueTraits.h"
#include "Rendering/CameraView.h"
#include "Rendering/SceneViewMode.h"
#include "Viewport/ViewportCamera.h"

#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	/** A way of shading the scene, and what commands call it. */
	struct FViewModeName
	{
		ESceneViewMode Mode;
		std::string_view Name;
	};

	constexpr std::array<FViewModeName, 3> ViewModeNames{{
		{ESceneViewMode::Lit, "Lit"},
		{ESceneViewMode::Unlit, "Unlit"},
		{ESceneViewMode::AmbientOcclusion, "AmbientOcclusion"},
	}};

	std::string_view GetViewModeName(ESceneViewMode Mode)
	{
		for (const FViewModeName& Entry : ViewModeNames)
		{
			if (Entry.Mode == Mode)
			{
				return Entry.Name;
			}
		}
		WW_CHECKF(false, "A view mode has no name for commands");
		return std::string_view();
	}

	std::optional<ESceneViewMode> FindViewMode(std::string_view Name)
	{
		for (const FViewModeName& Entry : ViewModeNames)
		{
			if (Entry.Name == Name)
			{
				return Entry.Mode;
			}
		}
		return std::nullopt;
	}

	/**
	 * Reads an argument that is a position.
	 *
	 * @return False, with the reason in OutError, if the argument is not three finite numbers.
	 */
	bool ReadPosition(
		const FCommandArguments& Arguments, std::string_view Name, FVector& OutPosition, std::string& OutError)
	{
		std::string Reason;
		const bool bIsVector = TPropertyValueTraits<FVector>::FromJson(Arguments.GetJson(Name), OutPosition, Reason);
		const bool bIsPosition =
			bIsVector && std::isfinite(OutPosition.x) && std::isfinite(OutPosition.y) && std::isfinite(OutPosition.z);
		if (!bIsPosition)
		{
			OutError = std::format("{} must be a position: three finite numbers, as [x, y, z].", Name);
		}
		return bIsPosition;
	}

	/**
	 * Reads an argument that is an angle or an amount, as a float.
	 *
	 * @return False, with the reason in OutError, if the argument is not a finite number that a float can hold.
	 */
	bool ReadNumber(const FCommandArguments& Arguments, std::string_view Name, float& OutNumber, std::string& OutError)
	{
		std::string Reason;
		if (!TPropertyValueTraits<float>::FromJson(Arguments.GetJson(Name), OutNumber, Reason) ||
			!std::isfinite(OutNumber))
		{
			OutError = std::format("{} must be a finite number.", Name);
			return false;
		}
		return true;
	}

	FCommandResult GetCamera(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		const FViewportCamera& Camera = Editor.GetViewportCamera();

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Location", TPropertyValueTraits<FVector>::ToJson(Camera.Location));
		Value.SetMember("Yaw", Camera.Yaw);
		Value.SetMember("Pitch", Camera.Pitch);
		Value.SetMember("FieldOfView", Camera.VerticalFieldOfView);
		Value.SetMember("Exposure", Camera.Exposure);
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult SetCamera(FEditor& Editor, const FCommandArguments& Arguments)
	{
		FViewportCamera Camera = Editor.GetViewportCamera();
		std::string Error;

		if (Arguments.Has("Location") && !ReadPosition(Arguments, "Location", Camera.Location, Error))
		{
			return FCommandResult::Failure(std::move(Error));
		}

		if (Arguments.Has("Yaw") && !ReadNumber(Arguments, "Yaw", Camera.Yaw, Error))
		{
			return FCommandResult::Failure(std::move(Error));
		}

		if (Arguments.Has("Pitch"))
		{
			if (!ReadNumber(Arguments, "Pitch", Camera.Pitch, Error))
			{
				return FCommandResult::Failure(std::move(Error));
			}
			if (std::abs(Camera.Pitch) > FViewportCamera::MaxPitch)
			{
				return FCommandResult::Failure(
					std::format("Pitch must be between -{0} and {0} degrees, and {1} was given.",
						FViewportCamera::MaxPitch,
						Camera.Pitch));
			}
		}

		if (Arguments.Has("LookAt"))
		{
			if (Arguments.Has("Yaw") || Arguments.Has("Pitch"))
			{
				return FCommandResult::Failure(
					"LookAt says which way the camera faces, so Yaw and Pitch cannot be given with it.");
			}

			FVector Target(0.0);
			if (!ReadPosition(Arguments, "LookAt", Target, Error))
			{
				return FCommandResult::Failure(std::move(Error));
			}
			if (!Camera.LookAt(Target))
			{
				return FCommandResult::Failure("The camera cannot look at the point it is at.");
			}
		}

		if (Arguments.Has("FieldOfView"))
		{
			if (!ReadNumber(Arguments, "FieldOfView", Camera.VerticalFieldOfView, Error))
			{
				return FCommandResult::Failure(std::move(Error));
			}
			const bool bIsInRange = Camera.VerticalFieldOfView >= FCameraView::MinFieldOfView &&
				Camera.VerticalFieldOfView <= FCameraView::MaxFieldOfView;
			if (!bIsInRange)
			{
				return FCommandResult::Failure(
					std::format("FieldOfView must be between {} and {} degrees, and {} was given.",
						FCameraView::MinFieldOfView,
						FCameraView::MaxFieldOfView,
						Camera.VerticalFieldOfView));
			}
		}

		if (Arguments.Has("Exposure"))
		{
			if (!ReadNumber(Arguments, "Exposure", Camera.Exposure, Error))
			{
				return FCommandResult::Failure(std::move(Error));
			}
			if (Camera.Exposure <= 0.0f)
			{
				return FCommandResult::Failure(
					std::format("Exposure must be greater than zero, and {} was given.", Camera.Exposure));
			}
		}

		Editor.SetViewportCamera(Camera);
		return FCommandResult::Success();
	}

	FCommandResult GetViewMode(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Mode", std::string(GetViewModeName(Editor.GetViewMode())));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult SetViewMode(FEditor& Editor, const FCommandArguments& Arguments)
	{
		const std::string& Name = Arguments.GetString("Mode");
		const std::optional<ESceneViewMode> Mode = FindViewMode(Name);
		if (!Mode.has_value())
		{
			return FCommandResult::Failure(std::format(
				"There is no view mode called '{}'. The view modes are Lit, Unlit and AmbientOcclusion.", Name));
		}

		Editor.SetViewMode(*Mode);
		return FCommandResult::Success();
	}
}

void FViewportCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Viewport.GetCamera",
			"Returns the point of view from which the viewport shows the scene: its Location, as [x, y, z] in metres; "
			"its Yaw, the degrees it is turned around the up axis, counter-clockwise seen from above, where 0 looks "
			"along -Z; its Pitch, the degrees it looks up from the horizon; its FieldOfView in degrees, from the top "
			"to the bottom of the picture; and its Exposure, by which the light of the scene is multiplied.",
			{},
			ECommandEffect::LeavesScene},
		GetCamera);

	Registry.Register(
		{"Viewport.SetCamera",
			"Moves the point of view from which the viewport shows the scene. What is left out stays as it is. The "
			"camera is the user's eye and not part of the scene: moving it cannot be undone and is not saved.",
			{FCommandParameter::Optional(
				 "Location", ECommandArgumentType::Json, "Where the camera is, as [x, y, z] in metres."),
				FCommandParameter::Optional("Yaw",
					ECommandArgumentType::Number,
					"The degrees the camera is turned around the up axis, counter-clockwise seen from above. At 0 it "
					"looks along -Z, at 90 along -X."),
				FCommandParameter::Optional("Pitch",
					ECommandArgumentType::Number,
					"The degrees the camera looks up from the horizon, negative for down. Between -89 and 89."),
				FCommandParameter::Optional("LookAt",
					ECommandArgumentType::Json,
					"A point, as [x, y, z], that the camera turns to face from where it is, or from Location if that "
					"is given. Takes the place of Yaw and Pitch."),
				FCommandParameter::Optional("FieldOfView",
					ECommandArgumentType::Number,
					"The angle between the top and the bottom of the picture, in degrees. Between 1 and 179."),
				FCommandParameter::Optional("Exposure",
					ECommandArgumentType::Number,
					"How bright the picture is: the light of the scene is multiplied by this. Greater than zero. A "
					"scene lit with real amounts of light, such as 100000 lux of sunlight, needs a small number.")},
			ECommandEffect::LeavesScene},
		SetCamera);

	Registry.Register({"Viewport.GetViewMode",
						  "Returns how the viewport shades the scene: Lit, Unlit or AmbientOcclusion.",
						  {},
						  ECommandEffect::LeavesScene},
		GetViewMode);

	Registry.Register(
		{"Viewport.SetViewMode",
			"Makes the viewport shade the scene in another way.",
			{FCommandParameter::Required("Mode",
				ECommandArgumentType::String,
				"Lit for the scene as its lights light it, which is the usual picture; Unlit for every surface in "
				"exactly its base colour; AmbientOcclusion for the ambient occlusion by itself, in shades of grey.")},
			ECommandEffect::LeavesScene},
		SetViewMode);
}
