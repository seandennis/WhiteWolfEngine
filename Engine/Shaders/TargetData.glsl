// What belongs to the picture being drawn, the render target, rather than to the scene: the ambient occlusion
// that was worked out for it before the lit pass. Descriptor set 2. Needs SceneData.glsl.

layout(set = 2, binding = 0) uniform texture2D AmbientOcclusionTexture;
layout(set = 2, binding = 128) uniform sampler AmbientOcclusionSampler;

// Returns how much of its surrounding the surface at the pixel being drawn can see, from 0 to 1. It is 1
// everywhere when ambient occlusion is switched off.
float GetAmbientOcclusion()
{
	return texture(sampler2D(AmbientOcclusionTexture, AmbientOcclusionSampler), gl_FragCoord.xy * View.ScreenParameters.xy).r;
}