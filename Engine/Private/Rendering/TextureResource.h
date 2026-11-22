#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"
#include "Rendering/Texture.h"

#include <nvrhi/nvrhi.h>

/** The copy of a texture's picture on the graphics card. */
struct FTextureResource
{
	/** The texture this is a copy of. Null once the texture is gone, which is when the copy is released. */
	TWeakObjectPtr<const WTexture> Texture;

	/** The version of the picture that was copied; see WTexture::GetImageVersion. */
	uint64 ImageVersion = 0;

	nvrhi::TextureHandle Handle;

	/** True until the picture has been written to the graphics card, which the next drawn picture does. */
	bool bNeedsUpload = true;
};
