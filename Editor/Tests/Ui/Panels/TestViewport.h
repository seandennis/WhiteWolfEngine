#pragma once

#include "CoreMinimal.h"

#include "Input/InputEvent.h"
#include "Input/MouseButton.h"
#include "Math/Vector.h"
#include "Rendering/CameraView.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/TestEditorUi.h"
#include "Ui/UiRectangle.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/vec4.hpp>

// What the tests of the viewport share: things to put into the scene that it shows, where they are in its picture,
// and a mouse that is held down on the picture and moved.

/** Adds an entity that shows a box of one metre, at a place in the world, and returns its identifier. */
inline int64 AddBox(FEditorUiTest& Test, const char* Name, const FVector& Location)
{
	WEntity* const Entity = Test.Editor.GetScene().CreateEntity(Name);
	REQUIRE(Entity != nullptr);
	Entity->SetWorldLocation(Location);
	WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
	REQUIRE(Component != nullptr);
	Component->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
	return Entity->GetId();
}

/** Returns where on the surface a point of the world is drawn, as the viewport's camera sees it now. */
inline FVector2D FindInViewport(const FEditorUiTest& Test, const FVector& WorldPoint)
{
	const FUiRectangle Picture = Test.RequireItem("Viewport");
	const FCameraView View = Test.Editor.GetViewportCamera().GetView();
	const float AspectRatio = Picture.GetWidth() / Picture.GetHeight();
	const glm::vec4 Clip = View.GetProjectionMatrix(AspectRatio) * View.GetViewMatrix() *
		glm::vec4(FVector3f(WorldPoint - View.Location), 1.0f);
	REQUIRE(Clip.w > 0.0f);

	// The camera counts from the middle of the picture and upwards, the surface from its top left corner.
	const float FromLeft = (Clip.x / Clip.w + 1.0f) * 0.5f * Picture.GetWidth();
	const float FromTop = (1.0f - Clip.y / Clip.w) * 0.5f * Picture.GetHeight();
	return FVector2D(static_cast<double>(Picture.Left + FromLeft), static_cast<double>(Picture.Top + FromTop));
}

/** Puts the mouse at a place on the surface and holds a button down there. */
inline void PressAt(FEditorUiTest& Test, const FVector2D& Position, EMouseButton Button)
{
	Test.MoveMouse(Position);
	Test.Input.HandleMouseButtonEvent(Button, EInputEvent::Pressed);
	Test.DrawFrame();
}

/** Puts the mouse on the middle of the viewport and holds a button down there. */
inline void PressOnViewport(FEditorUiTest& Test, EMouseButton Button)
{
	PressAt(Test, Test.RequireItem("Viewport").GetCenter(), Button);
}

/** Lets go of a button of the mouse. */
inline void Release(FEditorUiTest& Test, EMouseButton Button)
{
	Test.Input.HandleMouseButtonEvent(Button, EInputEvent::Released);
	Test.DrawFrame();
}

/** Moves the mouse from where it is, by a distance in pixels. */
inline void MoveBy(FEditorUiTest& Test, const FVector2D& Distance)
{
	Test.MoveMouse(Test.Input.GetMousePosition() + Distance);
}
