#include "Ui/UiInput.h"

#include "Input/InputState.h"
#include "Input/Key.h"
#include "Input/MouseButton.h"
#include "Ui/ImGuiLibrary.h"

#include <array>
#include <cfloat>

namespace
{
	/** A key of the engine and the key of Dear ImGui that it is. */
	struct FKeyPair
	{
		EKey Key;
		ImGuiKey UiKey;
	};

	constexpr std::array KeyPairs = {
		FKeyPair{EKey::Space, ImGuiKey_Space},
		FKeyPair{EKey::Apostrophe, ImGuiKey_Apostrophe},
		FKeyPair{EKey::Comma, ImGuiKey_Comma},
		FKeyPair{EKey::Minus, ImGuiKey_Minus},
		FKeyPair{EKey::Period, ImGuiKey_Period},
		FKeyPair{EKey::Slash, ImGuiKey_Slash},
		FKeyPair{EKey::Zero, ImGuiKey_0},
		FKeyPair{EKey::One, ImGuiKey_1},
		FKeyPair{EKey::Two, ImGuiKey_2},
		FKeyPair{EKey::Three, ImGuiKey_3},
		FKeyPair{EKey::Four, ImGuiKey_4},
		FKeyPair{EKey::Five, ImGuiKey_5},
		FKeyPair{EKey::Six, ImGuiKey_6},
		FKeyPair{EKey::Seven, ImGuiKey_7},
		FKeyPair{EKey::Eight, ImGuiKey_8},
		FKeyPair{EKey::Nine, ImGuiKey_9},
		FKeyPair{EKey::Semicolon, ImGuiKey_Semicolon},
		FKeyPair{EKey::Equal, ImGuiKey_Equal},
		FKeyPair{EKey::A, ImGuiKey_A},
		FKeyPair{EKey::B, ImGuiKey_B},
		FKeyPair{EKey::C, ImGuiKey_C},
		FKeyPair{EKey::D, ImGuiKey_D},
		FKeyPair{EKey::E, ImGuiKey_E},
		FKeyPair{EKey::F, ImGuiKey_F},
		FKeyPair{EKey::G, ImGuiKey_G},
		FKeyPair{EKey::H, ImGuiKey_H},
		FKeyPair{EKey::I, ImGuiKey_I},
		FKeyPair{EKey::J, ImGuiKey_J},
		FKeyPair{EKey::K, ImGuiKey_K},
		FKeyPair{EKey::L, ImGuiKey_L},
		FKeyPair{EKey::M, ImGuiKey_M},
		FKeyPair{EKey::N, ImGuiKey_N},
		FKeyPair{EKey::O, ImGuiKey_O},
		FKeyPair{EKey::P, ImGuiKey_P},
		FKeyPair{EKey::Q, ImGuiKey_Q},
		FKeyPair{EKey::R, ImGuiKey_R},
		FKeyPair{EKey::S, ImGuiKey_S},
		FKeyPair{EKey::T, ImGuiKey_T},
		FKeyPair{EKey::U, ImGuiKey_U},
		FKeyPair{EKey::V, ImGuiKey_V},
		FKeyPair{EKey::W, ImGuiKey_W},
		FKeyPair{EKey::X, ImGuiKey_X},
		FKeyPair{EKey::Y, ImGuiKey_Y},
		FKeyPair{EKey::Z, ImGuiKey_Z},
		FKeyPair{EKey::LeftBracket, ImGuiKey_LeftBracket},
		FKeyPair{EKey::Backslash, ImGuiKey_Backslash},
		FKeyPair{EKey::RightBracket, ImGuiKey_RightBracket},
		FKeyPair{EKey::GraveAccent, ImGuiKey_GraveAccent},
		FKeyPair{EKey::Escape, ImGuiKey_Escape},
		FKeyPair{EKey::Enter, ImGuiKey_Enter},
		FKeyPair{EKey::Tab, ImGuiKey_Tab},
		FKeyPair{EKey::Backspace, ImGuiKey_Backspace},
		FKeyPair{EKey::Insert, ImGuiKey_Insert},
		FKeyPair{EKey::Delete, ImGuiKey_Delete},
		FKeyPair{EKey::Right, ImGuiKey_RightArrow},
		FKeyPair{EKey::Left, ImGuiKey_LeftArrow},
		FKeyPair{EKey::Down, ImGuiKey_DownArrow},
		FKeyPair{EKey::Up, ImGuiKey_UpArrow},
		FKeyPair{EKey::PageUp, ImGuiKey_PageUp},
		FKeyPair{EKey::PageDown, ImGuiKey_PageDown},
		FKeyPair{EKey::Home, ImGuiKey_Home},
		FKeyPair{EKey::End, ImGuiKey_End},
		FKeyPair{EKey::CapsLock, ImGuiKey_CapsLock},
		FKeyPair{EKey::F1, ImGuiKey_F1},
		FKeyPair{EKey::F2, ImGuiKey_F2},
		FKeyPair{EKey::F3, ImGuiKey_F3},
		FKeyPair{EKey::F4, ImGuiKey_F4},
		FKeyPair{EKey::F5, ImGuiKey_F5},
		FKeyPair{EKey::F6, ImGuiKey_F6},
		FKeyPair{EKey::F7, ImGuiKey_F7},
		FKeyPair{EKey::F8, ImGuiKey_F8},
		FKeyPair{EKey::F9, ImGuiKey_F9},
		FKeyPair{EKey::F10, ImGuiKey_F10},
		FKeyPair{EKey::F11, ImGuiKey_F11},
		FKeyPair{EKey::F12, ImGuiKey_F12},
		FKeyPair{EKey::Numpad0, ImGuiKey_Keypad0},
		FKeyPair{EKey::Numpad1, ImGuiKey_Keypad1},
		FKeyPair{EKey::Numpad2, ImGuiKey_Keypad2},
		FKeyPair{EKey::Numpad3, ImGuiKey_Keypad3},
		FKeyPair{EKey::Numpad4, ImGuiKey_Keypad4},
		FKeyPair{EKey::Numpad5, ImGuiKey_Keypad5},
		FKeyPair{EKey::Numpad6, ImGuiKey_Keypad6},
		FKeyPair{EKey::Numpad7, ImGuiKey_Keypad7},
		FKeyPair{EKey::Numpad8, ImGuiKey_Keypad8},
		FKeyPair{EKey::Numpad9, ImGuiKey_Keypad9},
		FKeyPair{EKey::NumpadDecimal, ImGuiKey_KeypadDecimal},
		FKeyPair{EKey::NumpadDivide, ImGuiKey_KeypadDivide},
		FKeyPair{EKey::NumpadMultiply, ImGuiKey_KeypadMultiply},
		FKeyPair{EKey::NumpadSubtract, ImGuiKey_KeypadSubtract},
		FKeyPair{EKey::NumpadAdd, ImGuiKey_KeypadAdd},
		FKeyPair{EKey::NumpadEnter, ImGuiKey_KeypadEnter},
		FKeyPair{EKey::LeftShift, ImGuiKey_LeftShift},
		FKeyPair{EKey::LeftControl, ImGuiKey_LeftCtrl},
		FKeyPair{EKey::LeftAlt, ImGuiKey_LeftAlt},
		FKeyPair{EKey::LeftSuper, ImGuiKey_LeftSuper},
		FKeyPair{EKey::RightShift, ImGuiKey_RightShift},
		FKeyPair{EKey::RightControl, ImGuiKey_RightCtrl},
		FKeyPair{EKey::RightAlt, ImGuiKey_RightAlt},
		FKeyPair{EKey::RightSuper, ImGuiKey_RightSuper},
	};

	/** A mouse button of the engine and its number in Dear ImGui. */
	struct FMouseButtonPair
	{
		EMouseButton Button;
		ImGuiMouseButton UiButton;
	};

	constexpr std::array MouseButtonPairs = {
		FMouseButtonPair{EMouseButton::Left, ImGuiMouseButton_Left},
		FMouseButtonPair{EMouseButton::Right, ImGuiMouseButton_Right},
		FMouseButtonPair{EMouseButton::Middle, ImGuiMouseButton_Middle},
		FMouseButtonPair{EMouseButton::Extra1, 3},
		FMouseButtonPair{EMouseButton::Extra2, 4},
	};

	/**
	 * Reports a key or button as down or up through Send. One that went both ways within the frame is first
	 * reported in the state it passed through, which is the opposite of the one it ended in. Dear ImGui ignores a
	 * report that changes nothing, so the state can be reported every frame.
	 */
	template <typename SendType>
	void ReportState(bool bIsDown, bool bWasPressed, bool bWasReleased, const SendType& Send)
	{
		if (bWasPressed && bWasReleased)
		{
			Send(!bIsDown);
		}
		Send(bIsDown);
	}
}

void FUiInput::Feed(const FInputState& Input, ImGuiIO& Io)
{
	// The modifiers come before the keys they modify, as they do from Dear ImGui's own platform code.
	const bool bIsControlDown = Input.IsKeyDown(EKey::LeftControl) || Input.IsKeyDown(EKey::RightControl);
	const bool bIsShiftDown = Input.IsKeyDown(EKey::LeftShift) || Input.IsKeyDown(EKey::RightShift);
	const bool bIsAltDown = Input.IsKeyDown(EKey::LeftAlt) || Input.IsKeyDown(EKey::RightAlt);
	const bool bIsSuperDown = Input.IsKeyDown(EKey::LeftSuper) || Input.IsKeyDown(EKey::RightSuper);
	Io.AddKeyEvent(ImGuiMod_Ctrl, bIsControlDown);
	Io.AddKeyEvent(ImGuiMod_Shift, bIsShiftDown);
	Io.AddKeyEvent(ImGuiMod_Alt, bIsAltDown);
	Io.AddKeyEvent(ImGuiMod_Super, bIsSuperDown);

	for (const FKeyPair& Pair : KeyPairs)
	{
		ReportState(Input.IsKeyDown(Pair.Key),
			Input.WasKeyPressed(Pair.Key),
			Input.WasKeyReleased(Pair.Key),
			[&Io, &Pair](bool bIsDown) { Io.AddKeyEvent(Pair.UiKey, bIsDown); });
	}

	// Where the mouse is, before what its buttons did there.
	if (Input.HasMousePosition())
	{
		const FVector2D& Position = Input.GetMousePosition();
		Io.AddMousePosEvent(static_cast<float>(Position.x), static_cast<float>(Position.y));
	}
	else
	{
		// Dear ImGui's way of saying that there is no mouse over the user interface.
		Io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
	}

	for (const FMouseButtonPair& Pair : MouseButtonPairs)
	{
		ReportState(Input.IsMouseButtonDown(Pair.Button),
			Input.WasMouseButtonPressed(Pair.Button),
			Input.WasMouseButtonReleased(Pair.Button),
			[&Io, &Pair](bool bIsDown) { Io.AddMouseButtonEvent(Pair.UiButton, bIsDown); });
	}

	const FVector2D& Scroll = Input.GetScrollDelta();
	if (Scroll.x != 0.0 || Scroll.y != 0.0)
	{
		Io.AddMouseWheelEvent(static_cast<float>(Scroll.x), static_cast<float>(Scroll.y));
	}

	if (!Input.GetTypedText().empty())
	{
		Io.AddInputCharactersUTF8(Input.GetTypedText().c_str());
	}
}
