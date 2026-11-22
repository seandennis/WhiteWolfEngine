#pragma once

#include "CoreMinimal.h"

class FRenderTarget;
class WTexture;

/**
 * A run of triangles of a user interface that are covered with the same picture and clipped to the same rectangle;
 * see FUiDrawList.
 */
struct FUiDrawCommand
{
	/** The place of the command's first index in FUiDrawList::Indices. */
	int32 FirstIndex = 0;

	/** How many indices the command has: three for each triangle. */
	int32 IndexCount = 0;

	/** Added to each of the command's indices to give the place of a vertex in FUiDrawList::Vertices. */
	int32 VertexOffset = 0;

	// The rectangle outside of which nothing of the command is drawn, in pixels from the top left corner of the
	// surface: ClipLeft and ClipTop are the first column and row inside it, ClipRight and ClipBottom the first
	// beyond it. A rectangle that reaches beyond the surface is cut off at its edges.

	int32 ClipLeft = 0;
	int32 ClipTop = 0;
	int32 ClipRight = 0;
	int32 ClipBottom = 0;

	/**
	 * The picture that covers the triangles and multiplies their colours. Null, or a texture without a picture,
	 * leaves the colours as they are.
	 */
	const WTexture* Texture = nullptr;

	/**
	 * A render target whose surface covers the triangles instead of a texture, as an editor's viewport shows a
	 * picture of the scene. What the surface holds as alpha is ignored: it covers what lies behind completely. At
	 * most one of Texture and RenderTarget is set, and a render target cannot be drawn onto itself.
	 */
	const FRenderTarget* RenderTarget = nullptr;
};
