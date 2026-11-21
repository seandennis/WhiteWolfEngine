// The textures of the material of the mesh being drawn, and the samplers that say how each is read. A material
// without a texture of some kind gets a white one, which changes nothing, since textures multiply the material's
// plain values. Textures are bound from 0 on and samplers from 128 on; see the binding offsets of NVRHI's Vulkan
// backend. The order is that of the slots in Engine/Private/Rendering/MaterialTextureSlot.h.

layout(set = 1, binding = 0) uniform texture2D BaseColorTexture;
layout(set = 1, binding = 1) uniform texture2D MetallicRoughnessTexture;
layout(set = 1, binding = 2) uniform texture2D NormalTexture;
layout(set = 1, binding = 3) uniform texture2D EmissiveTexture;
layout(set = 1, binding = 4) uniform texture2D OcclusionTexture;

layout(set = 1, binding = 128) uniform sampler BaseColorSampler;
layout(set = 1, binding = 129) uniform sampler MetallicRoughnessSampler;
layout(set = 1, binding = 130) uniform sampler NormalSampler;
layout(set = 1, binding = 131) uniform sampler EmissiveSampler;
layout(set = 1, binding = 132) uniform sampler OcclusionSampler;