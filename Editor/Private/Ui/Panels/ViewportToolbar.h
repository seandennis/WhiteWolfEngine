#pragma once

#include "CoreMinimal.h"

#include "Math/Vector.h"

#include <array>

struct FUiFrame;

/**
 * The buttons that lie over the top of the viewport's picture: what dragging the gizmo does and along which axes,
 * whether things are dragged in steps and in which, how fast the camera flies, and how the scene is shaded.
 *
 * None of that is part of the scene. It is how the user works, and the buttons set it on the editor directly
 * (FEditor::SetGizmoSettings, SetViewportCamera and SetViewMode), as steering the camera does.
 *
 * The buttons are in groups, which stay on one row while the picture is wide enough for them and go on in further
 * rows where it is not.
 */
class FViewportToolbar
{
public:
	/**
	 * Lays the toolbar out over the picture of the scene, from its top left corner. The toolbar is a window of its
	 * own inside the panel: where the mouse is on the toolbar, it is not on the picture.
	 *
	 * @param PictureCorner Where the top left corner of the picture is on the surface, in pixels.
	 * @param PictureSize The width and the height of the picture, in pixels.
	 */
	void Draw(FUiFrame& Frame, const FVector2f& PictureCorner, const FVector2f& PictureSize);

private:
	/** How many groups of buttons the toolbar has. */
	static constexpr int32 GroupCount = 5;

	/**
	 * How wide each group was when it was last laid out, in pixels: what says whether a group still fits on the
	 * row of the one before it.
	 */
	std::array<float, GroupCount> GroupWidths = {};
};
