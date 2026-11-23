#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"
#include "Ui/ImGuiLibrary.h"

#include <string>
#include <vector>

class FRenderTarget;
class WTexture;

/** What a picture of the user interface is to the engine: a texture or the surface of a render target. */
struct FUiPicture
{
	const WTexture* Texture = nullptr;
	const FRenderTarget* RenderTarget = nullptr;
};

/**
 * The pictures of the user interface: what stands behind the texture identifiers of Dear ImGui.
 *
 * Dear ImGui refers to every picture it draws with a number. Some are its own: the textures in which it keeps the
 * shapes of the characters of its fonts, which it asks to have created, updated and destroyed. Those become
 * WTexture objects that this class keeps alive. The others are pictures that a panel shows for the length of one
 * frame, such as the render target of the viewport.
 */
class FUiTextures
{
public:
	FUiTextures() = default;

	/** Every texture must have been given back with ReleaseAll. */
	~FUiTextures();

	FUiTextures(const FUiTextures&) = delete;
	FUiTextures& operator=(const FUiTextures&) = delete;

	/** Forgets the pictures that panels showed in the last frame. */
	void BeginFrame();

	/** Returns the identifier under which widgets show a render target during the current frame. */
	ImTextureID AddRenderTarget(const FRenderTarget& Target);

	/**
	 * Does what Dear ImGui asks for the textures it owns: creates, updates and destroys them. Call it after
	 * ImGui::Render and before the frame is drawn.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return False if a texture could not take the picture Dear ImGui has for it.
	 */
	bool Update(ImVector<ImTextureData*>& UiTextures, std::string& OutError);

	/** Destroys every texture that Dear ImGui owns. Call it before its context is destroyed. */
	void ReleaseAll(ImVector<ImTextureData*>& UiTextures);

	/** Returns the picture that an identifier stands for, or neither kind if it stands for none. */
	FUiPicture Find(ImTextureID Identifier) const;

private:
	/** Returns the texture of Dear ImGui's that an identifier stands for, or null if it stands for none. */
	WTexture* FindOwnedTexture(ImTextureID Identifier) const;

	/** Makes a texture show what Dear ImGui has in UiTexture now. */
	bool CopyPixels(const ImTextureData& UiTexture, WTexture& Texture, std::string& OutError) const;

	void Destroy(ImTextureData& UiTexture);

	/**
	 * The textures of Dear ImGui, each in the root set for as long as it is here. The identifier of a texture is
	 * its place in this list plus one, because Dear ImGui keeps zero for "no texture". A place whose texture was
	 * destroyed is null and is used again.
	 */
	std::vector<TWeakObjectPtr<WTexture>> OwnedTextures;

	/** The pictures of the current frame. The identifier of one is its place in this list plus FirstFramePicture. */
	std::vector<FUiPicture> FramePictures;
};
