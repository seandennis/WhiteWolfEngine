#include "Input/InputState.h"

#include <bitset>
#include <cstddef>

namespace
{
	/**
	 * Applies a press or release to the three sets that describe a group of keys or buttons. Index must be in
	 * range.
	 */
	template <std::size_t Count>
	void ApplyEvent(std::bitset<Count>& Down,
		std::bitset<Count>& Pressed,
		std::bitset<Count>& Released,
		std::size_t Index,
		EInputEvent Event)
	{
		const bool bIsDown = Down.test(Index);
		if (Event == EInputEvent::Pressed && !bIsDown)
		{
			Down.set(Index);
			Pressed.set(Index);
		}
		else if (Event == EInputEvent::Released && bIsDown)
		{
			Down.reset(Index);
			Released.set(Index);
		}
	}
}

void FInputState::BeginFrame()
{
	KeysPressed.reset();
	KeysReleased.reset();
	MouseButtonsPressed.reset();
	MouseButtonsReleased.reset();
	MouseDelta = FVector2D(0.0);
	ScrollDelta = FVector2D(0.0);
}

void FInputState::HandleKeyEvent(EKey Key, EInputEvent Event)
{
	const std::size_t Index = static_cast<std::size_t>(Key);
	if (Index < KeyCount)
	{
		ApplyEvent(KeysDown, KeysPressed, KeysReleased, Index, Event);
	}
}

void FInputState::HandleMouseButtonEvent(EMouseButton Button, EInputEvent Event)
{
	const std::size_t Index = static_cast<std::size_t>(Button);
	if (Index < MouseButtonCount)
	{
		ApplyEvent(MouseButtonsDown, MouseButtonsPressed, MouseButtonsReleased, Index, Event);
	}
}

void FInputState::HandleMouseMove(const FVector2D& Position)
{
	if (bHasMousePosition)
	{
		MouseDelta += Position - MousePosition;
	}

	MousePosition = Position;
	bHasMousePosition = true;
}

void FInputState::HandleScroll(const FVector2D& Delta)
{
	ScrollDelta += Delta;
}

void FInputState::ReleaseAll()
{
	KeysReleased |= KeysDown;
	KeysDown.reset();
	MouseButtonsReleased |= MouseButtonsDown;
	MouseButtonsDown.reset();
	bHasMousePosition = false;
}

bool FInputState::IsKeyDown(EKey Key) const
{
	const std::size_t Index = static_cast<std::size_t>(Key);
	return Index < KeyCount && KeysDown.test(Index);
}

bool FInputState::WasKeyPressed(EKey Key) const
{
	const std::size_t Index = static_cast<std::size_t>(Key);
	return Index < KeyCount && KeysPressed.test(Index);
}

bool FInputState::WasKeyReleased(EKey Key) const
{
	const std::size_t Index = static_cast<std::size_t>(Key);
	return Index < KeyCount && KeysReleased.test(Index);
}

bool FInputState::IsMouseButtonDown(EMouseButton Button) const
{
	const std::size_t Index = static_cast<std::size_t>(Button);
	return Index < MouseButtonCount && MouseButtonsDown.test(Index);
}

bool FInputState::WasMouseButtonPressed(EMouseButton Button) const
{
	const std::size_t Index = static_cast<std::size_t>(Button);
	return Index < MouseButtonCount && MouseButtonsPressed.test(Index);
}

bool FInputState::WasMouseButtonReleased(EMouseButton Button) const
{
	const std::size_t Index = static_cast<std::size_t>(Button);
	return Index < MouseButtonCount && MouseButtonsReleased.test(Index);
}
