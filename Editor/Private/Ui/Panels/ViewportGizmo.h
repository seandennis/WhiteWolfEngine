#pragma once

#include "CoreMinimal.h"

#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Viewport/GizmoDrag.h"
#include "Viewport/GizmoHandle.h"
#include "Viewport/GizmoMode.h"

#include <optional>
#include <vector>

class FTransformGizmo;
struct FUiFrame;

/**
 * The gizmo as the viewport shows it: the handles that lie on the picture of the scene, on the entity that was
 * selected last, with which the user moves, turns and sizes what is selected.
 *
 * Where the handles are, and what dragging one of them does, FTransformGizmo and FGizmoDrag work out. This class
 * draws the handles, lets the mouse take hold of them, and gives the editor what a drag comes to: in every frame
 * of it, a transform for each entity that is selected and not below another that is, all of them one step of the
 * history.
 */
class FViewportGizmo
{
public:
	/**
	 * Lays the gizmo out over the picture of the scene, which has to be the item that was laid out last, and
	 * carries out what the mouse does with it.
	 *
	 * @param PictureCorner Where the top left corner of the picture is on the surface, in pixels.
	 * @param PictureSize The width and the height of the picture, in pixels.
	 */
	void Draw(FUiFrame& Frame, const FVector2f& PictureCorner, const FVector2f& PictureSize);

	/**
	 * Takes the mouse from the gizmo and ends the drag that is under way, if one is. This is what a frame does
	 * that does not lay the picture out, whatever takes the picture away.
	 */
	void LetGo(FUiFrame& Frame);

	/**
	 * Returns true if the mouse was the gizmo's in the frame that was laid out last: from the press of the button
	 * that takes a handle to the frame in which the button comes up, that one included. For that long the mouse
	 * does nothing else on the picture.
	 */
	bool HadMouse() const
	{
		return bHadMouse;
	}

private:
	/** An entity that a drag moves, as it was when the drag began. */
	struct FDraggedEntity
	{
		int64 EntityId = 0;

		/** Its transform relative to its parent, and in the world. */
		FTransform RelativeTransform;
		FTransform WorldTransform;

		/** The transform of its parent in the world; none for an entity at the root of the scene. */
		FTransform ParentTransform;
	};

	/** Begins a drag of a handle, of everything that is selected, as a step of the history. */
	void BeginDrag(FUiFrame& Frame,
		const FTransformGizmo& Gizmo,
		EGizmoMode Mode,
		EGizmoHandle Handle,
		const FVector2f& MousePoint);

	/** Takes the drag to where the mouse is, and gives each entity that it moves the transform it then has. */
	void ContinueDrag(FUiFrame& Frame, const FVector2f& MousePoint);

	/** Ends the drag, and with it the step of the history. Does nothing if no drag is under way. */
	void EndDrag(FUiFrame& Frame);

	/** Forgets the drag without ending a step: for a drag whose step something else has ended already. */
	void DropDrag();

	/** Returns the transform that the drag has given an entity so far, relative to the entity's parent. */
	FTransform FindDraggedTransform(const FDraggedEntity& Dragged) const;

	/** The drag that is under way, if one is. */
	std::optional<FGizmoDrag> Drag;

	/** What was selected when that drag began, and those of it that the drag moves. */
	std::vector<int64> DraggedSelection;
	std::vector<FDraggedEntity> DraggedEntities;

	/** True from the press of the button that takes a handle until the button comes up. */
	bool bHasMouse = false;

	/** See HadMouse. */
	bool bHadMouse = false;
};
