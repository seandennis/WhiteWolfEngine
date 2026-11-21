#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialTextureSlot.h"

#include <nvrhi/nvrhi.h>

#include <array>

/** What gives the shaders the textures of one material. */
struct FMaterialBindings
{
	/** The material this belongs to. Null once the material is gone, which is when this is released. */
	TWeakObjectPtr<WMaterial> Material;

	/**
	 * The textures and samplers the binding set was made with, one per EMaterialTextureSlot. The set is made
	 * again when the material's textures, their pictures or their wrap modes no longer give these.
	 */
	std::array<nvrhi::ITexture*, MaterialTextureSlotCount> Textures{};
	std::array<nvrhi::ISampler*, MaterialTextureSlotCount> Samplers{};

	nvrhi::BindingSetHandle BindingSet;
};
