#pragma once

#include "CoreMinimal.h"

class FInputState;
struct ImGuiIO;

/** Tells Dear ImGui what the keyboard and mouse did, from the engine's input state. */
class FUiInput
{
public:
	/**
	 * Passes on one frame of input: the keys and mouse buttons that are down, where the mouse is, how far the wheel
	 * turned and what was typed. A key or button that went down and came up again within the frame is passed on as
	 * both, so that a quick tap is not lost.
	 */
	static void Feed(const FInputState& Input, ImGuiIO& Io);
};
