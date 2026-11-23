#pragma once

#include "CoreMinimal.h"

#include "Math/Vector.h"
#include "Ui/Panels/ViewportGizmo.h"
#include "Ui/Panels/ViewportToolbar.h"

#include <memory>
#include <string>

class FEditor;
class FRenderDevice;
class FRenderTarget;
class FSceneRenderer;
struct FUiFrame;

/**
 * The panel that shows the scene being edited, as the editor's viewport camera sees it, and in which the user
 * steers that camera.
 *
 * The scene is drawn onto a render target of the panel's own, as large as the panel, and the user interface shows
 * that surface as a picture.
 *
 * The camera is steered as in Unreal: with the right mouse button held, the mouse looks around and W, A, S, D, Q
 * and E fly, at a speed that the wheel changes; with Alt, the left button takes the camera around what it looks
 * at and the right one moves it closer or away; the middle button moves it sideways and up; and the wheel by
 * itself moves it closer or away in steps.
 *
 * A click with the left button selects the entity whose mesh is seen under the mouse, and a click on nothing
 * selects nothing. With Ctrl held, a click adds the entity to what is selected or takes it out. What is selected
 * has an orange line around it in the picture, and the gizmo on it (FViewportGizmo), whose handles the left button
 * drags to move, turn and size it.
 *
 * Over the top of the picture lies a toolbar (FViewportToolbar) that sets what the gizmo does, how fast the camera
 * flies and how the scene is shaded.
 */
class FViewportPanel
{
public:
	/** What the panel is called on its tab. Dear ImGui also knows the panel by this. */
	static constexpr const char* Title = "Viewport";

	/**
	 * Lays the panel out: a picture of the scene that fills it. The picture itself is drawn afterwards, by
	 * RenderScene.
	 *
	 * @param Device Where the surface for the picture is created when the panel has changed its size.
	 * @param OutError Receives the reason on failure.
	 * @return False if the graphics card could not create the surface.
	 */
	bool Draw(FUiFrame& Frame, FRenderDevice& Device, std::string& OutError);

	/**
	 * Draws the scene onto the surface that the picture shows, if the last call of Draw laid a picture out. Call
	 * it after the frame is laid out and before the user interface is drawn.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return False if the scene could not be drawn.
	 */
	bool RenderScene(FSceneRenderer& Renderer, const FEditor& Editor, std::string& OutError);

	/**
	 * Returns true while the user is steering the camera with a mouse button held down. For that long the mouse
	 * should be the panel's alone and free to move without end (FWindow::CaptureMouse).
	 */
	bool IsNavigating() const
	{
		return Navigation != ENavigation::None;
	}

private:
	/** The ways the camera is steered while a mouse button is held down. */
	enum class ENavigation : uint8
	{
		None,
		/** The mouse looks around and the keys move the camera. */
		Fly,
		/** The mouse takes the camera around its focus point. */
		Orbit,
		/** The mouse moves the camera towards its focus point or away from it. */
		Dolly,
		/** The mouse moves the camera sideways and up, and the scene with the mouse. */
		Pan,
	};

	/**
	 * Steers the editor's viewport camera by what the mouse and the keyboard do on the picture, which is the item
	 * that was laid out last.
	 *
	 * @param PictureHeight The height of the picture in pixels.
	 * @param Continued How the camera was steered in the last frame, which goes on while the button stays down.
	 */
	void Navigate(FUiFrame& Frame, int32 PictureHeight, ENavigation Continued);

	/**
	 * Selects what the user has clicked on in the picture, which is the item that was laid out last, if the left
	 * mouse button has just come up from a click on it.
	 *
	 * @param PictureCorner Where the top left corner of the picture is on the surface, in pixels.
	 * @param PictureSize The width and the height of the picture, in pixels.
	 * @param Continued How the camera was steered in the last frame: a press that steered it was no click.
	 */
	void SelectClicked(
		FUiFrame& Frame, const FVector2f& PictureCorner, const FVector2f& PictureSize, ENavigation Continued);

	/** How the camera is being steered, as of the frame that was laid out last. */
	ENavigation Navigation = ENavigation::None;

	/** The handles on what is selected, and the drag of one of them that is under way. */
	FViewportGizmo Gizmo;

	/** The buttons over the top of the picture. */
	FViewportToolbar Toolbar;

	/** The surface the scene is drawn onto. Null until the panel is first shown. */
	std::unique_ptr<FRenderTarget> Target;

	/** True if the frame that was laid out last shows the picture. */
	bool bIsPictureShown = false;
};
