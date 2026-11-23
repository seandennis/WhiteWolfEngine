#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Rendering/Image.h"

#include <memory>
#include <string>

class FRenderContext;

/**
 * A surface the renderer draws on, in memory on the graphics card: what a window shows, or what a screenshot is
 * taken of. Its pixels are 8 bits per channel and sRGB-encoded, ready to display or save; the graphics card does
 * the encoding, so colours are drawn to it in linear space. With it come a depth buffer, which lets nearer surfaces
 * hide farther ones, and a high dynamic range buffer, in which the renderer lights the scene before it fits the
 * result to the display.
 *
 * Render targets are created by FRenderDevice::CreateRenderTarget. A render target stays usable after the
 * FRenderDevice object that created it is gone; the graphics card is released with the last thing that uses it.
 */
class FRenderTarget
{
private:
	struct FImplementation;

public:
	/** The largest width or height a render target can have, in pixels. */
	static constexpr int32 MaxDimension = 16384;

	/** Used by FRenderDevice. Other code cannot call it, because it cannot name the implementation type. */
	explicit FRenderTarget(std::unique_ptr<FImplementation> InImplementation);

	ENGINE_API ~FRenderTarget();

	FRenderTarget(const FRenderTarget&) = delete;
	FRenderTarget& operator=(const FRenderTarget&) = delete;

	ENGINE_API int32 GetWidth() const;
	ENGINE_API int32 GetHeight() const;

	/** Fills the whole surface with one colour, given in linear space. The depth buffer is left as it is. */
	ENGINE_API void Clear(const FLinearColor& Color);

	/**
	 * Copies the surface from the graphics card into an image. This waits for the graphics card to finish drawing,
	 * so it is for screenshots and tests, not for every frame.
	 *
	 * @param OutImage Receives the pixels, sRGB-encoded. Unchanged on failure.
	 * @param OutError Receives the reason on failure.
	 * @return True if the image was read.
	 */
	ENGINE_API bool ReadPixels(FImage& OutImage, std::string& OutError) const;

private:
	friend class FRenderDevice;
	friend class FSceneRenderer;
	friend class FSwapChain;
	friend class FUiRenderer;

	std::unique_ptr<FImplementation> Implementation;
};
