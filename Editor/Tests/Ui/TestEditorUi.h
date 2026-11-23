#pragma once

#include "CoreMinimal.h"

#include "Editor.h"
#include "Input/InputEvent.h"
#include "Input/InputState.h"
#include "Input/Key.h"
#include "Input/MouseButton.h"
#include "Rendering/Image.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/TestRendering.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "Ui/EditorUi.h"
#include "Ui/UiRectangle.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

/**
 * An editor with its user interface, for one test: drawn onto a surface of the test's own, with rendering
 * validated, and driven by pressing keys and moving a mouse that exist only here.
 *
 * Every action is followed by the frames that the user interface needs to take it in, so a test reads like what a
 * person does: Click("Outliner/3"), then look at what is selected.
 */
class FEditorUiTest
{
public:
	/** The size of the surface, in pixels: that of a small window. */
	static constexpr int32 DefaultWidth = 1280;
	static constexpr int32 DefaultHeight = 720;

	/** The time from one frame to the next, in seconds. */
	static constexpr double FrameSeconds = 1.0 / 60.0;

	explicit FEditorUiTest(int32 Width = DefaultWidth, int32 Height = DefaultHeight)
	{
		std::string Error;
		Ui = FEditorUi::Create(Editor, *Device, Error);
		INFO("Error: ", Error);
		REQUIRE(Ui != nullptr);
		Target = CreateTestTarget(*Device, Width, Height);
		DrawFrames(SettlingFrameCount);
	}

	FEditorUiTest(const FEditorUiTest&) = delete;
	FEditorUiTest& operator=(const FEditorUiTest&) = delete;

	/** Draws one frame with the input that was given since the last one, which the test expects to work. */
	void DrawFrame()
	{
		std::string Error;
		const bool bDrawn = Ui->Draw(*Target, Input, FrameSeconds, Error);
		INFO("Error: ", Error);
		REQUIRE(bDrawn);
		Input.BeginFrame();
	}

	void DrawFrames(int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			DrawFrame();
		}
	}

	/** Returns the picture of the user interface as the last frame drew it. */
	FImage ReadPicture() const
	{
		return ReadTestTarget(*Target);
	}

	/** Returns where an item was drawn in the last frame; the test fails if it was not. */
	FUiRectangle RequireItem(std::string_view Name) const
	{
		const std::optional<FUiRectangle> Rectangle = Ui->FindItem(Name);
		INFO("Item: ", std::string(Name));
		REQUIRE(Rectangle.has_value());
		return *Rectangle;
	}

	bool HasItem(std::string_view Name) const
	{
		return Ui->FindItem(Name).has_value();
	}

	/** Moves the mouse to a place on the surface and lets the user interface see it there. */
	void MoveMouse(const FVector2D& Position)
	{
		Input.HandleMouseMove(Position);
		DrawFrame();
	}

	/** Clicks a mouse button at a place on the surface. */
	void ClickAt(const FVector2D& Position, EMouseButton Button = EMouseButton::Left)
	{
		MoveMouse(Position);
		Input.HandleMouseButtonEvent(Button, EInputEvent::Pressed);
		DrawFrame();
		Input.HandleMouseButtonEvent(Button, EInputEvent::Released);
		DrawFrames(SettlingFrameCount);
	}

	/** Clicks in the middle of the item with the given name, which has to be in sight. */
	void Click(std::string_view Name, EMouseButton Button = EMouseButton::Left)
	{
		ClickAt(RequireItem(Name).GetCenter(), Button);
	}

	/** Drags with the left mouse button from the middle of one item to the middle of another, and lets go there. */
	void Drag(std::string_view FromName, std::string_view ToName)
	{
		DragBetween(RequireItem(FromName).GetCenter(), RequireItem(ToName).GetCenter());
	}

	/** Drags with the left mouse button from the middle of an item by a distance in pixels, and lets go there. */
	void DragBy(std::string_view Name, const FVector2D& Distance)
	{
		const FVector2D From = RequireItem(Name).GetCenter();
		DragBetween(From, From + Distance);
	}

	/** Drags with the left mouse button from one place on the surface to another, and lets go there. */
	void DragBetween(const FVector2D& From, const FVector2D& To)
	{
		MoveMouse(From);
		Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		DrawFrame();

		// The mouse crosses the distance over several frames, as a hand does: a drag begins once it has moved
		// some way with the button down, and what it is over has to see it arrive.
		for (int32 Step = 1; Step <= DragStepCount; ++Step)
		{
			MoveMouse(From + (To - From) * (static_cast<double>(Step) / DragStepCount));
		}
		DrawFrame();
		Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Released);
		DrawFrames(SettlingFrameCount);
	}

	/** Presses and releases a key while the given other keys, such as Ctrl, are held down. */
	void PressKey(EKey Key, std::initializer_list<EKey> HeldKeys = {})
	{
		for (const EKey HeldKey : HeldKeys)
		{
			Input.HandleKeyEvent(HeldKey, EInputEvent::Pressed);
		}
		DrawFrame();
		Input.HandleKeyEvent(Key, EInputEvent::Pressed);
		DrawFrame();
		Input.HandleKeyEvent(Key, EInputEvent::Released);
		DrawFrame();
		for (const EKey HeldKey : HeldKeys)
		{
			Input.HandleKeyEvent(HeldKey, EInputEvent::Released);
		}
		DrawFrames(SettlingFrameCount);
	}

	/** Holds a key down, or lets it go, from the next frame on. */
	void SetKeyDown(EKey Key, bool bIsDown)
	{
		Input.HandleKeyEvent(Key, bIsDown ? EInputEvent::Pressed : EInputEvent::Released);
		DrawFrame();
	}

	/** Types text, as a keyboard does: characters, not keys. Only for text of the ASCII range. */
	void Type(std::string_view Text)
	{
		for (const char Character : Text)
		{
			Input.HandleTypedCharacter(static_cast<char32_t>(Character));
		}
		DrawFrames(SettlingFrameCount);
	}

private:
	/**
	 * How many frames are drawn after something was done, so that what it caused is laid out and drawn: a window
	 * that opens is measured in one frame and placed in the next.
	 */
	static constexpr int32 SettlingFrameCount = 3;

	/** In how many steps a dragged mouse crosses the distance. */
	static constexpr int32 DragStepCount = 5;

	// Declared in the order they are created, and destroyed in the opposite one: the user interface before the
	// editor and the render device that it uses.

	FScopedEditorTest EditorTest;

public:
	FScopedLogCapture Capture;
	std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
	FEditor Editor;
	std::unique_ptr<FEditorUi> Ui;
	std::unique_ptr<FRenderTarget> Target;
	FInputState Input;
};
