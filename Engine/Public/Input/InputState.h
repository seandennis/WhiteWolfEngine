#pragma once

#include "CoreMinimal.h"

#include "Input/InputEvent.h"
#include "Input/Key.h"
#include "Input/MouseButton.h"

#include <bitset>
#include <cstddef>

/**
 * The state of the keyboard and mouse as one window sees it: which keys and buttons are down, which went down or
 * came up during the current frame, where the mouse is and how far it and the scroll wheel moved.
 *
 * The state changes through the Handle functions. A window calls them with the events the operating system
 * reports; tests and tools call them to simulate input, which is how the engine is driven without a person at the
 * keyboard. BeginFrame separates one frame's events from the next.
 */
class FInputState
{
public:
	/**
	 * Starts a new frame: forgets which keys and buttons were pressed or released during the previous frame and
	 * resets the mouse and scroll deltas. Keys that are held stay down. Call it once per frame, before the frame's
	 * events are handled.
	 */
	ENGINE_API void BeginFrame();

	/**
	 * Records that a key went down or came up. Pressing a key that is already down, or releasing one that is up,
	 * changes nothing. A key outside the range of EKey is ignored.
	 */
	ENGINE_API void HandleKeyEvent(EKey Key, EInputEvent Event);

	/** Records that a mouse button went down or came up, with the same rules as HandleKeyEvent. */
	ENGINE_API void HandleMouseButtonEvent(EMouseButton Button, EInputEvent Event);

	/**
	 * Records the position of the mouse, in pixels from the top left corner of the window's content. The first
	 * position after the state is created or reset sets the position without counting as movement.
	 */
	ENGINE_API void HandleMouseMove(const FVector2D& Position);

	/** Records movement of the scroll wheel; y is the usual vertical wheel, positive away from the user. */
	ENGINE_API void HandleScroll(const FVector2D& Delta);

	/**
	 * Releases every key and button that is down, each counting as released in the current frame, and forgets the
	 * mouse position. A window does this when it loses the keyboard focus, because the releases then go to
	 * another window and would otherwise never be seen.
	 */
	ENGINE_API void ReleaseAll();

	/** Returns true while the key is held down. */
	ENGINE_API bool IsKeyDown(EKey Key) const;

	/** Returns true if the key went down during the current frame, even if it has come up again since. */
	ENGINE_API bool WasKeyPressed(EKey Key) const;

	/** Returns true if the key came up during the current frame. */
	ENGINE_API bool WasKeyReleased(EKey Key) const;

	ENGINE_API bool IsMouseButtonDown(EMouseButton Button) const;
	ENGINE_API bool WasMouseButtonPressed(EMouseButton Button) const;
	ENGINE_API bool WasMouseButtonReleased(EMouseButton Button) const;

	/** Returns the last known position of the mouse, or zero if it has not been seen yet. */
	const FVector2D& GetMousePosition() const
	{
		return MousePosition;
	}

	/** Returns how far the mouse has moved during the current frame. */
	const FVector2D& GetMouseDelta() const
	{
		return MouseDelta;
	}

	/** Returns how far the scroll wheel has moved during the current frame. */
	const FVector2D& GetScrollDelta() const
	{
		return ScrollDelta;
	}

private:
	static constexpr std::size_t KeyCount = static_cast<std::size_t>(EKey::Last) + 1;
	static constexpr std::size_t MouseButtonCount = static_cast<std::size_t>(EMouseButton::Last) + 1;

	std::bitset<KeyCount> KeysDown;
	std::bitset<KeyCount> KeysPressed;
	std::bitset<KeyCount> KeysReleased;

	std::bitset<MouseButtonCount> MouseButtonsDown;
	std::bitset<MouseButtonCount> MouseButtonsPressed;
	std::bitset<MouseButtonCount> MouseButtonsReleased;

	FVector2D MousePosition = FVector2D(0.0);
	FVector2D MouseDelta = FVector2D(0.0);
	FVector2D ScrollDelta = FVector2D(0.0);

	/** False until the first mouse position arrives, so that it is not mistaken for a movement from zero. */
	bool bHasMousePosition = false;
};
