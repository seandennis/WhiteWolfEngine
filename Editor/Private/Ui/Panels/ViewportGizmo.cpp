#include "Ui/Panels/ViewportGizmo.h"

#include "Editor.h"
#include "Math/Color.h"
#include "Math/Quat.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiRectangle.h"
#include "Ui/UiTheme.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/GizmoShape.h"
#include "Viewport/GizmoSpace.h"
#include "Viewport/TransformGizmo.h"
#include "Viewport/ViewportCamera.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/** A handle of the gizmo, and what it is called in the name of the place where the mouse takes hold of it. */
	struct FHandleName
	{
		EGizmoHandle Handle;
		std::string_view Name;
	};

	constexpr std::array<FHandleName, 7> HandleNames = {{
		{EGizmoHandle::X, "X"},
		{EGizmoHandle::Y, "Y"},
		{EGizmoHandle::Z, "Z"},
		{EGizmoHandle::YZ, "YZ"},
		{EGizmoHandle::XZ, "XZ"},
		{EGizmoHandle::XY, "XY"},
		{EGizmoHandle::All, "All"},
	}};

	/** How much of what is behind it the square of a plane hides, of 255, and how much the rest of the gizmo. */
	constexpr uint8 PlaneSquareOpacity = 96;
	constexpr uint8 FullOpacity = 255;

	/** How thick the line around the square of a plane is, as a part of the thickness of the gizmo's other lines. */
	constexpr float PlaneSquareLineWidth = 0.5f;

	/** How far the place that a handle is named at reaches to each side of the handle's point: half a pixel. */
	constexpr float HandlePlaceHalfSize = 0.5f;

	const FQuat NoRotation(1.0f, 0.0f, 0.0f, 0.0f);

	/** What the history calls a drag that makes copies of what is selected, and moves them. */
	constexpr const char* CopyStepName = "Duplicate";

	/** Returns what the history calls a drag of the gizmo. */
	std::string GetStepName(EGizmoMode Mode)
	{
		switch (Mode)
		{
			case EGizmoMode::Rotate:
				return "Rotate";
			case EGizmoMode::Scale:
				return "Scale";
			case EGizmoMode::Translate:
			default:
				return "Move";
		}
	}

	ImU32 MakeDrawColor(FColor Color, uint8 Opacity)
	{
		return IM_COL32(Color.R, Color.G, Color.B, Opacity);
	}

	/**
	 * Returns the colour of a handle: that of its axis, the same that marks the numbers of a vector in the Details
	 * panel. The square of a plane has the colour of the axis that it leaves out, and the handle of all three axes
	 * the colour of none.
	 */
	FColor GetHandleColor(EGizmoHandle Handle)
	{
		constexpr int32 NoAxis = 3;
		return FUiTheme::GetPartColor(FTransformGizmo::GetAxisIndex(Handle).value_or(NoAxis));
	}

	/**
	 * Returns twice the area that corners are around: above zero if they go clockwise on a surface that counts
	 * downwards, and below zero if they go the other way.
	 */
	float GetTwiceArea(const ImVector<ImVec2>& Corners)
	{
		float Sum = 0.0f;
		for (int Index = 0; Index < Corners.Size; ++Index)
		{
			const ImVec2& From = Corners[Index];
			const ImVec2& To = Corners[(Index + 1) % Corners.Size];
			Sum += From.x * To.y - To.x * From.y;
		}
		return Sum;
	}

	/** Draws the shapes of a gizmo over the picture, the handle that the mouse has in the colour that says so. */
	void DrawShapes(ImDrawList& DrawList,
		const std::vector<FGizmoShape>& Shapes,
		EGizmoHandle Highlighted,
		const FVector2f& PictureCorner)
	{
		const float LineWidth = FUiTheme::GetGizmoLineWidth();
		ImVector<ImVec2> Points;

		// The handle that the mouse has is drawn last, over the others where they cross it.
		for (const bool bDrawsHighlighted : {false, true})
		{
			for (const FGizmoShape& Shape : Shapes)
			{
				const bool bIsHighlighted = Shape.Handle == Highlighted;
				if (bIsHighlighted != bDrawsHighlighted)
				{
					continue;
				}

				Points.resize(0);
				for (const FVector2f& Point : Shape.Points)
				{
					Points.push_back(ImVec2(PictureCorner.x + Point.x, PictureCorner.y + Point.y));
				}

				const FColor Color = bIsHighlighted ? FUiTheme::GetGizmoHighlightColor() : GetHandleColor(Shape.Handle);
				if (!Shape.bIsArea)
				{
					DrawList.AddPolyline(Points.Data, Points.Size, MakeDrawColor(Color, FullOpacity), LineWidth);
					continue;
				}

				// Dear ImGui smooths the edge of a filled shape outwards only if its corners go clockwise.
				if (GetTwiceArea(Points) < 0.0f)
				{
					std::reverse(Points.begin(), Points.end());
				}

				// The square of a plane lets what is behind it be seen, and has a line around it.
				const bool bIsPlane = FTransformGizmo::IsPlaneHandle(Shape.Handle);
				DrawList.AddConvexPolyFilled(
					Points.Data, Points.Size, MakeDrawColor(Color, bIsPlane ? PlaneSquareOpacity : FullOpacity));
				if (bIsPlane)
				{
					DrawList.AddPolyline(Points.Data,
						Points.Size,
						MakeDrawColor(Color, FullOpacity),
						LineWidth * PlaneSquareLineWidth,
						ImDrawFlags_Closed);
				}
			}
		}
	}

	/**
	 * Gives the gizmo and its handles the names that a test finds them by: "Viewport/Gizmo" is the square around
	 * the middle of the gizmo that its axes reach the sides of, and "Viewport/Gizmo/X" a point at which the mouse
	 * has the handle X, if that point is in the picture.
	 */
	void NameHandles(FUiItems& Items,
		const FTransformGizmo& Gizmo,
		EGizmoMode Mode,
		const FVector2f& PictureCorner,
		const FVector2f& PictureSize)
	{
		const auto MakePlace = [&PictureCorner](const FVector2f& PicturePoint, float HalfSize)
		{
			const FVector2f Middle = PictureCorner + PicturePoint;
			FUiRectangle Place;
			Place.Left = Middle.x - HalfSize;
			Place.Top = Middle.y - HalfSize;
			Place.Right = Middle.x + HalfSize;
			Place.Bottom = Middle.y + HalfSize;
			return Place;
		};

		if (const std::optional<FVector2f> Middle = Gizmo.FindInPicture(Gizmo.GetPivot()))
		{
			Items.Add("Viewport/Gizmo", MakePlace(*Middle, Gizmo.GetAxisLength()));
		}

		for (const FHandleName& HandleName : HandleNames)
		{
			const std::optional<FVector2f> Point = Gizmo.FindHandlePoint(Mode, HandleName.Handle);
			const bool bIsInPicture = Point.has_value() && Point->x >= 0.0f && Point->y >= 0.0f &&
				Point->x < PictureSize.x && Point->y < PictureSize.y;
			if (bIsInPicture)
			{
				Items.Add(std::format("Viewport/Gizmo/{}", HandleName.Name), MakePlace(*Point, HandlePlaceHalfSize));
			}
		}
	}
}

void FViewportGizmo::Draw(FUiFrame& Frame, const FVector2f& PictureCorner, const FVector2f& PictureSize)
{
	FEditor& Editor = Frame.Editor;
	const ImGuiIO& Io = ImGui::GetIO();
	const FVector2f MousePoint = FVector2f(Io.MousePos.x, Io.MousePos.y) - PictureCorner;
	const bool bIsMouseKnown = ImGui::IsMousePosValid();

	// A drag moves what was selected when it began. Once something else is selected, or one of those things is
	// gone, as a command of an agent may have it in the middle of a drag, the drag is over.
	if (Drag.has_value() && Editor.GetSelection() != DraggedSelection)
	{
		EndDrag(Frame);
	}

	// The mouse is the gizmo's until the button that took a handle comes up. Where the mouse is when it does is
	// where the drag ends, so the drag is taken there before it is ended.
	bHadMouse = bHasMouse;
	bHasMouse = bHasMouse && ImGui::IsItemActive();
	if (Drag.has_value() && bIsMouseKnown)
	{
		ContinueDrag(Frame, MousePoint);
	}
	if (!bHasMouse)
	{
		EndDrag(Frame);
	}

	// The gizmo is on the entity that was selected last, and with nothing selected there is none. What is
	// selected is asked now, after the drag: a drag of copies selects them when it makes them.
	const std::vector<int64> Selection = Editor.GetSelection();
	const WEntity* const PivotEntity = Selection.empty() ? nullptr : Editor.GetScene().FindEntityById(Selection.back());
	if (PivotEntity == nullptr)
	{
		return;
	}

	// A drag goes on doing what it began to do, whatever the gizmo is set to in the meantime. Sizes are changed
	// along a thing's own axes, whichever axes things are moved and turned along.
	const FGizmoSettings& Settings = Editor.GetGizmoSettings();
	const EGizmoMode Mode = Drag.has_value() ? Drag->GetMode() : Settings.Mode;
	const bool bHasOwnAxes = Settings.Space == EGizmoSpace::Local || Mode == EGizmoMode::Scale;
	const FTransform PivotTransform = PivotEntity->GetWorldTransform();
	const FTransformGizmo Gizmo(Editor.GetViewportCamera().GetView(),
		PictureSize,
		PivotTransform.Location,
		bHasOwnAxes ? PivotTransform.Rotation : NoRotation,
		FUiTheme::GetGizmoAxisLength());

	// The mouse takes a handle with its left button. A button that is down on the picture already is doing
	// something else there.
	EGizmoHandle Highlighted = Drag.has_value() ? Drag->GetHandle() : EGizmoHandle::None;
	const bool bIsPressed = ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	const bool bIsMouseFree = !ImGui::IsItemActive() || bIsPressed;
	if (!Drag.has_value() && bIsMouseFree && bIsMouseKnown && ImGui::IsItemHovered())
	{
		Highlighted = Gizmo.FindHandle(Mode, MousePoint, FUiTheme::GetGizmoReach());
		if (bIsPressed && Highlighted != EGizmoHandle::None)
		{
			BeginDrag(Frame, Gizmo, Mode, Highlighted, MousePoint);
		}
	}

	ImDrawList& DrawList = *ImGui::GetWindowDrawList();
	constexpr bool bWithinPanel = true;
	DrawList.PushClipRect(ImVec2(PictureCorner.x, PictureCorner.y),
		ImVec2(PictureCorner.x + PictureSize.x, PictureCorner.y + PictureSize.y),
		bWithinPanel);
	DrawShapes(DrawList, Gizmo.GetShapes(Mode), Highlighted, PictureCorner);
	DrawList.PopClipRect();
	NameHandles(Frame.Items, Gizmo, Mode, PictureCorner, PictureSize);
}

void FViewportGizmo::LetGo(FUiFrame& Frame)
{
	EndDrag(Frame);
	bHasMouse = false;
	bHadMouse = false;
}

void FViewportGizmo::BeginDrag(
	FUiFrame& Frame, const FTransformGizmo& Gizmo, EGizmoMode Mode, EGizmoHandle Handle, const FVector2f& MousePoint)
{
	FEditor& Editor = Frame.Editor;
	const WScene& Scene = Editor.GetScene();

	// What is below a thing that is dragged goes with it, and is not dragged a second time by itself.
	DraggedSelection = Editor.GetSelection();
	DraggedEntities.clear();
	for (const int64 EntityId : FUiCommands::GetTopmostEntities(Editor, DraggedSelection))
	{
		const WEntity* const Entity = Scene.FindEntityById(EntityId);
		FDraggedEntity Dragged;
		Dragged.EntityId = EntityId;
		Dragged.RelativeTransform = Entity->GetRelativeTransform();
		Dragged.WorldTransform = Entity->GetWorldTransform();
		if (const WEntity* const Parent = Entity->GetParent())
		{
			Dragged.ParentTransform = Parent->GetWorldTransform();
		}
		DraggedEntities.push_back(Dragged);
	}

	// With Alt held, the drag is of copies, and the history calls it by what it makes.
	Drag.emplace(Gizmo, Mode, Handle, MousePoint);
	bWantsCopies = ImGui::GetIO().KeyAlt;
	bHasMouse = true;
	bHadMouse = true;
	FUiCommands::BeginChange(Editor, bWantsCopies ? CopyStepName : GetStepName(Mode));
}

void FViewportGizmo::ContinueDrag(FUiFrame& Frame, const FVector2f& MousePoint)
{
	FEditor& Editor = Frame.Editor;

	// Whatever else the user does in the middle of a drag, such as undoing, ends the step of the drag. The drag
	// is then over: the scene is no longer the one that it began in.
	if (!Editor.IsStepOpen())
	{
		DropDrag();
		return;
	}

	// Copies are made when the drag first comes to something. A press that goes nowhere leaves no copy lying on
	// what it is a copy of, where nobody would see it.
	Drag->MoveTo(MousePoint, Editor.GetGizmoSettings());
	if (bWantsCopies)
	{
		if (!HasDragMovedAnything())
		{
			return;
		}
		MakeCopies(Frame);
	}

	for (const FDraggedEntity& Dragged : DraggedEntities)
	{
		// A command that changes nothing is not worth giving.
		const WEntity* const Entity = Editor.GetScene().FindEntityById(Dragged.EntityId);
		const FTransform Transform = FindDraggedTransform(Dragged);
		if (Entity != nullptr && !(Entity->GetRelativeTransform() == Transform))
		{
			FUiCommands::SetEntityTransform(Editor, Dragged.EntityId, Transform);
		}
	}
}

void FViewportGizmo::EndDrag(FUiFrame& Frame)
{
	if (Drag.has_value())
	{
		FUiCommands::EndChange(Frame.Editor);
		DropDrag();
	}
}

void FViewportGizmo::DropDrag()
{
	Drag.reset();
	DraggedSelection.clear();
	DraggedEntities.clear();
	bWantsCopies = false;
}

bool FViewportGizmo::HasDragMovedAnything() const
{
	return std::ranges::any_of(DraggedEntities,
		[this](const FDraggedEntity& Dragged)
		{ return !(FindDraggedTransform(Dragged) == Dragged.RelativeTransform); });
}

void FViewportGizmo::MakeCopies(FUiFrame& Frame)
{
	FEditor& Editor = Frame.Editor;
	std::vector<int64> Originals;
	for (const FDraggedEntity& Dragged : DraggedEntities)
	{
		Originals.push_back(Dragged.EntityId);
	}

	// A copy is made beside what it is a copy of, below the same parent, so it begins where the original was
	// when the drag began. The copies are selected, which makes them what the drag is of from here on.
	const std::vector<std::optional<int64>> Copies = FUiCommands::CopyEntities(Editor, Originals);
	std::vector<FDraggedEntity> DraggedCopies;
	for (std::size_t Index = 0; Index < DraggedEntities.size(); ++Index)
	{
		if (Copies[Index].has_value())
		{
			FDraggedEntity Copy = DraggedEntities[Index];
			Copy.EntityId = *Copies[Index];
			DraggedCopies.push_back(Copy);
		}
	}
	DraggedEntities = std::move(DraggedCopies);
	DraggedSelection = Editor.GetSelection();
	bWantsCopies = false;
}

FTransform FViewportGizmo::FindDraggedTransform(const FDraggedEntity& Dragged) const
{
	// Where the drag has taken the thing in the world says where it is relative to its parent. What the drag has
	// left alone stays exactly as it was: a drag that has done nothing yet changes nothing, and moving a thing
	// does not put its rotation through the way into the world and back.
	const FTransform InWorld = Drag->Apply(Dragged.WorldTransform);
	FTransform Transform = Dragged.RelativeTransform;
	if (InWorld.Location != Dragged.WorldTransform.Location)
	{
		Transform.Location = Dragged.ParentTransform.InverseTransformPosition(InWorld.Location);
	}
	if (InWorld.Rotation != Dragged.WorldTransform.Rotation)
	{
		Transform.Rotation = glm::normalize(glm::conjugate(Dragged.ParentTransform.Rotation) * InWorld.Rotation);
	}

	// A size is changed along the thing's own axes, which its size relative to its parent is along too.
	Transform.Scale = Dragged.RelativeTransform.Scale * Drag->GetScale();
	return Transform;
}
