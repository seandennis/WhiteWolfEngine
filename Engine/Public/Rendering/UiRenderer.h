#pragma once

#include "CoreMinimal.h"

#include <memory>
#include <string>

class FRenderDevice;
class FRenderTarget;
struct FUiDrawList;

/**
 * Draws user interfaces: the triangles of an FUiDrawList onto a render target, over what the target shows.
 *
 * Colours are blended in linear light. The colours of the vertices are sRGB-encoded and are decoded before they
 * are blended; a texture is read as its colour space says.
 *
 * It keeps the pictures of the textures it has drawn on the graphics card, copies one again when WTexture::SetImage
 * has given the texture another picture, and releases it when the texture has been collected. It is used from the
 * main thread.
 */
class FUiRenderer
{
private:
	struct FImplementation;

public:
	/**
	 * Creates a renderer for a render device.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return The renderer, or null if the graphics card could not create what it needs.
	 */
	ENGINE_API static std::unique_ptr<FUiRenderer> Create(FRenderDevice& Device, std::string& OutError);

	/** Used by Create. Other code cannot call it, because it cannot name the implementation type. */
	explicit FUiRenderer(std::unique_ptr<FImplementation> InImplementation);

	ENGINE_API ~FUiRenderer();

	FUiRenderer(const FUiRenderer&) = delete;
	FUiRenderer& operator=(const FUiRenderer&) = delete;

	/**
	 * Draws a list onto a render target. The target must belong to the render device this renderer was created
	 * for, and so must every render target the list shows.
	 *
	 * The list is refused as a whole, and nothing drawn, if it is not consistent: an index count that is not a
	 * multiple of three, indices or vertices that a command names and the list does not have, a negative number,
	 * a command with both a texture and a render target, or one that shows the target it is drawn onto.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return True if the list was drawn. A list without commands draws nothing and succeeds.
	 */
	ENGINE_API bool Draw(const FUiDrawList& DrawList, FRenderTarget& Target, std::string& OutError);

private:
	std::unique_ptr<FImplementation> Implementation;
};
