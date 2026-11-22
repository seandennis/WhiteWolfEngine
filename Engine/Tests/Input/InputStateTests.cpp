#include "Input/InputState.h"

#include <doctest/doctest.h>

#include <string>

TEST_SUITE("FInputState")
{
	TEST_CASE("A new input state has nothing down, nothing typed and no movement")
	{
		const FInputState Input;

		CHECK_FALSE(Input.IsKeyDown(EKey::W));
		CHECK_FALSE(Input.WasKeyPressed(EKey::W));
		CHECK_FALSE(Input.WasKeyReleased(EKey::W));
		CHECK_FALSE(Input.IsMouseButtonDown(EMouseButton::Left));
		CHECK(Input.GetTypedText().empty());
		CHECK_FALSE(Input.HasMousePosition());
		CHECK(Input.GetMousePosition() == FVector2D(0.0));
		CHECK(Input.GetMouseDelta() == FVector2D(0.0));
		CHECK(Input.GetScrollDelta() == FVector2D(0.0));
	}

	TEST_CASE("A pressed key is down and counts as pressed in that frame")
	{
		FInputState Input;

		Input.HandleKeyEvent(EKey::W, EInputEvent::Pressed);

		CHECK(Input.IsKeyDown(EKey::W));
		CHECK(Input.WasKeyPressed(EKey::W));
		CHECK_FALSE(Input.WasKeyReleased(EKey::W));
		CHECK_FALSE(Input.IsKeyDown(EKey::S));
	}

	TEST_CASE("A held key stays down in the next frame but is no longer newly pressed")
	{
		FInputState Input;
		Input.HandleKeyEvent(EKey::W, EInputEvent::Pressed);

		Input.BeginFrame();

		CHECK(Input.IsKeyDown(EKey::W));
		CHECK_FALSE(Input.WasKeyPressed(EKey::W));
	}

	TEST_CASE("A released key is up and counts as released in that frame only")
	{
		FInputState Input;
		Input.HandleKeyEvent(EKey::W, EInputEvent::Pressed);
		Input.BeginFrame();

		Input.HandleKeyEvent(EKey::W, EInputEvent::Released);
		CHECK_FALSE(Input.IsKeyDown(EKey::W));
		CHECK(Input.WasKeyReleased(EKey::W));

		Input.BeginFrame();
		CHECK_FALSE(Input.WasKeyReleased(EKey::W));
	}

	TEST_CASE("A key pressed and released within one frame is seen as both")
	{
		FInputState Input;

		Input.HandleKeyEvent(EKey::Space, EInputEvent::Pressed);
		Input.HandleKeyEvent(EKey::Space, EInputEvent::Released);

		CHECK_FALSE(Input.IsKeyDown(EKey::Space));
		CHECK(Input.WasKeyPressed(EKey::Space));
		CHECK(Input.WasKeyReleased(EKey::Space));
	}

	TEST_CASE("Pressing a key that is down, or releasing one that is up, changes nothing")
	{
		FInputState Input;
		Input.HandleKeyEvent(EKey::A, EInputEvent::Pressed);
		Input.BeginFrame();

		Input.HandleKeyEvent(EKey::A, EInputEvent::Pressed);
		Input.HandleKeyEvent(EKey::D, EInputEvent::Released);

		CHECK(Input.IsKeyDown(EKey::A));
		CHECK_FALSE(Input.WasKeyPressed(EKey::A));
		CHECK_FALSE(Input.IsKeyDown(EKey::D));
		CHECK_FALSE(Input.WasKeyReleased(EKey::D));
	}

	TEST_CASE("Keys are tracked independently, across the whole range of key codes")
	{
		FInputState Input;

		Input.HandleKeyEvent(EKey::Space, EInputEvent::Pressed);
		Input.HandleKeyEvent(EKey::LeftControl, EInputEvent::Pressed);
		Input.HandleKeyEvent(EKey::RightSuper, EInputEvent::Pressed);
		Input.HandleKeyEvent(EKey::LeftControl, EInputEvent::Released);

		CHECK(Input.IsKeyDown(EKey::Space));
		CHECK_FALSE(Input.IsKeyDown(EKey::LeftControl));
		CHECK(Input.IsKeyDown(EKey::RightSuper));
		CHECK_FALSE(Input.IsKeyDown(EKey::RightControl));
	}

	TEST_CASE("A key code outside the range of EKey is ignored")
	{
		FInputState Input;
		const EKey OutOfRange = static_cast<EKey>(static_cast<uint16>(EKey::Last) + 1);
		const EKey FarOutOfRange = static_cast<EKey>(60000);

		Input.HandleKeyEvent(OutOfRange, EInputEvent::Pressed);
		Input.HandleKeyEvent(FarOutOfRange, EInputEvent::Pressed);

		CHECK_FALSE(Input.IsKeyDown(OutOfRange));
		CHECK_FALSE(Input.WasKeyPressed(FarOutOfRange));
		CHECK_FALSE(Input.WasKeyReleased(FarOutOfRange));
	}

	TEST_CASE("Typed characters add up to text, in the order they were typed")
	{
		FInputState Input;

		Input.HandleTypedCharacter(U'H');
		Input.HandleTypedCharacter(U'i');
		Input.HandleTypedCharacter(U' ');
		Input.HandleTypedCharacter(U'!');

		CHECK(Input.GetTypedText() == "Hi !");
	}

	TEST_CASE("Typed text is UTF-8, whatever the character")
	{
		FInputState Input;

		Input.HandleTypedCharacter(U'\xE9');
		Input.HandleTypedCharacter(U'\x20AC');
		Input.HandleTypedCharacter(U'\x1F600');
		Input.HandleTypedCharacter(U'\xA0');

		CHECK(Input.GetTypedText() == "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xC2\xA0");
	}

	TEST_CASE("What was typed is forgotten when the next frame begins")
	{
		FInputState Input;
		Input.HandleTypedCharacter(U'a');

		Input.BeginFrame();
		CHECK(Input.GetTypedText().empty());

		Input.HandleTypedCharacter(U'b');
		CHECK(Input.GetTypedText() == "b");
	}

	TEST_CASE("Control characters are keys, not text")
	{
		FInputState Input;

		Input.HandleTypedCharacter(U'\0');
		Input.HandleTypedCharacter(U'\b');
		Input.HandleTypedCharacter(U'\t');
		Input.HandleTypedCharacter(U'\n');
		Input.HandleTypedCharacter(U'\r');
		Input.HandleTypedCharacter(U'\x1B');
		Input.HandleTypedCharacter(U'\x1F');
		Input.HandleTypedCharacter(U'\x7F');
		Input.HandleTypedCharacter(U'\x80');
		Input.HandleTypedCharacter(U'\x9F');

		CHECK(Input.GetTypedText().empty());
	}

	TEST_CASE("A value that is not a character is not text")
	{
		FInputState Input;

		Input.HandleTypedCharacter(static_cast<char32_t>(0xD800));
		Input.HandleTypedCharacter(static_cast<char32_t>(0xDFFF));
		Input.HandleTypedCharacter(static_cast<char32_t>(0x110000));
		Input.HandleTypedCharacter(static_cast<char32_t>(0xFFFFFFFF));

		CHECK(Input.GetTypedText().empty());
	}

	TEST_CASE("Typing and pressing keys are told apart")
	{
		FInputState Input;

		Input.HandleKeyEvent(EKey::A, EInputEvent::Pressed);
		CHECK(Input.GetTypedText().empty());

		Input.HandleTypedCharacter(U'w');
		CHECK_FALSE(Input.IsKeyDown(EKey::W));
		CHECK_FALSE(Input.WasKeyPressed(EKey::W));
		CHECK(Input.GetTypedText() == "w");
	}

	TEST_CASE("Mouse buttons follow the same rules as keys")
	{
		FInputState Input;

		Input.HandleMouseButtonEvent(EMouseButton::Right, EInputEvent::Pressed);
		CHECK(Input.IsMouseButtonDown(EMouseButton::Right));
		CHECK(Input.WasMouseButtonPressed(EMouseButton::Right));
		CHECK_FALSE(Input.IsMouseButtonDown(EMouseButton::Left));

		Input.BeginFrame();
		CHECK(Input.IsMouseButtonDown(EMouseButton::Right));
		CHECK_FALSE(Input.WasMouseButtonPressed(EMouseButton::Right));

		Input.HandleMouseButtonEvent(EMouseButton::Right, EInputEvent::Released);
		CHECK_FALSE(Input.IsMouseButtonDown(EMouseButton::Right));
		CHECK(Input.WasMouseButtonReleased(EMouseButton::Right));
	}

	TEST_CASE("A mouse button code outside the range of EMouseButton is ignored")
	{
		FInputState Input;
		const EMouseButton OutOfRange = static_cast<EMouseButton>(200);

		Input.HandleMouseButtonEvent(OutOfRange, EInputEvent::Pressed);

		CHECK_FALSE(Input.IsMouseButtonDown(OutOfRange));
		CHECK_FALSE(Input.WasMouseButtonPressed(OutOfRange));
		CHECK_FALSE(Input.WasMouseButtonReleased(OutOfRange));
	}

	TEST_CASE("The first mouse position is not a movement")
	{
		FInputState Input;

		Input.HandleMouseMove(FVector2D(400.0, 300.0));

		CHECK(Input.HasMousePosition());
		CHECK(Input.GetMousePosition() == FVector2D(400.0, 300.0));
		CHECK(Input.GetMouseDelta() == FVector2D(0.0));
	}

	TEST_CASE("Once the mouse has left, its position is not known and its return is not a movement")
	{
		FInputState Input;
		Input.HandleMouseMove(FVector2D(630.0, 300.0));
		Input.BeginFrame();

		Input.HandleMouseLeave();
		CHECK_FALSE(Input.HasMousePosition());
		CHECK(Input.GetMousePosition() == FVector2D(630.0, 300.0));
		CHECK(Input.GetMouseDelta() == FVector2D(0.0));

		Input.HandleMouseMove(FVector2D(10.0, 20.0));
		CHECK(Input.HasMousePosition());
		CHECK(Input.GetMousePosition() == FVector2D(10.0, 20.0));
		CHECK(Input.GetMouseDelta() == FVector2D(0.0));

		Input.HandleMouseMove(FVector2D(12.0, 21.0));
		CHECK(Input.GetMouseDelta() == FVector2D(2.0, 1.0));
	}

	TEST_CASE("The mouse leaving keeps the movement of that frame and the buttons that are down")
	{
		FInputState Input;
		Input.HandleMouseMove(FVector2D(100.0, 100.0));
		Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Input.HandleMouseMove(FVector2D(90.0, 100.0));

		Input.HandleMouseLeave();

		CHECK(Input.GetMouseDelta() == FVector2D(-10.0, 0.0));
		CHECK(Input.IsMouseButtonDown(EMouseButton::Left));
	}

	TEST_CASE("Mouse movement adds up within a frame and starts again in the next")
	{
		FInputState Input;
		Input.HandleMouseMove(FVector2D(100.0, 100.0));

		Input.HandleMouseMove(FVector2D(110.0, 95.0));
		Input.HandleMouseMove(FVector2D(125.0, 90.0));
		CHECK(Input.GetMousePosition() == FVector2D(125.0, 90.0));
		CHECK(Input.GetMouseDelta() == FVector2D(25.0, -10.0));

		Input.BeginFrame();
		CHECK(Input.GetMouseDelta() == FVector2D(0.0));
		CHECK(Input.GetMousePosition() == FVector2D(125.0, 90.0));

		Input.HandleMouseMove(FVector2D(120.0, 90.0));
		CHECK(Input.GetMouseDelta() == FVector2D(-5.0, 0.0));
	}

	TEST_CASE("Scrolling adds up within a frame and starts again in the next")
	{
		FInputState Input;

		Input.HandleScroll(FVector2D(0.0, 1.0));
		Input.HandleScroll(FVector2D(0.5, 2.0));
		CHECK(Input.GetScrollDelta() == FVector2D(0.5, 3.0));

		Input.BeginFrame();
		CHECK(Input.GetScrollDelta() == FVector2D(0.0));
	}

	TEST_CASE("ReleaseAll releases every key and button, each seen as released in that frame")
	{
		FInputState Input;
		Input.HandleKeyEvent(EKey::W, EInputEvent::Pressed);
		Input.HandleKeyEvent(EKey::LeftShift, EInputEvent::Pressed);
		Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Input.BeginFrame();

		Input.ReleaseAll();

		CHECK_FALSE(Input.IsKeyDown(EKey::W));
		CHECK_FALSE(Input.IsKeyDown(EKey::LeftShift));
		CHECK_FALSE(Input.IsMouseButtonDown(EMouseButton::Left));
		CHECK(Input.WasKeyReleased(EKey::W));
		CHECK(Input.WasKeyReleased(EKey::LeftShift));
		CHECK(Input.WasMouseButtonReleased(EMouseButton::Left));
		CHECK_FALSE(Input.WasKeyReleased(EKey::S));
	}

	TEST_CASE("ReleaseAll leaves what was typed in that frame")
	{
		FInputState Input;
		Input.HandleTypedCharacter(U'x');

		Input.ReleaseAll();

		CHECK(Input.GetTypedText() == "x");
	}

	TEST_CASE("After ReleaseAll the next mouse position is not a movement")
	{
		FInputState Input;
		Input.HandleMouseMove(FVector2D(10.0, 10.0));
		Input.BeginFrame();

		Input.ReleaseAll();
		CHECK_FALSE(Input.HasMousePosition());
		Input.HandleMouseMove(FVector2D(500.0, 500.0));

		CHECK(Input.HasMousePosition());
		CHECK(Input.GetMousePosition() == FVector2D(500.0, 500.0));
		CHECK(Input.GetMouseDelta() == FVector2D(0.0));
	}
}
