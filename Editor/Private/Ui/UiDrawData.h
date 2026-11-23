#pragma once

#include "CoreMinimal.h"

class FUiTextures;
struct FUiDrawList;
struct ImDrawData;

/** Turns what Dear ImGui has laid out into what the engine draws. */
class FUiDrawData
{
public:
	/**
	 * Writes the triangles of a frame of Dear ImGui as one list for FUiRenderer.
	 *
	 * @param DrawData What ImGui::GetDrawData returned after ImGui::Render.
	 * @param Textures Says which texture or render target each of Dear ImGui's texture identifiers stands for. A
	 * command whose identifier stands for nothing is drawn without a picture.
	 * @param OutList Receives the list; what it held is replaced.
	 */
	static void Convert(const ImDrawData& DrawData, const FUiTextures& Textures, FUiDrawList& OutList);
};
