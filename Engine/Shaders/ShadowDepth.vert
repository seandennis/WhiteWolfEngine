#version 460

// Draws a static mesh into a shadow map: from the light's point of view, and only its depth.

#include "SceneData.glsl"

layout(push_constant) uniform FShadowPassConstants
{
	// Which of View.ShadowMatrices this shadow map uses.
	int MatrixIndex;
} ShadowPass;

layout(location = 0) in vec3 InPosition;

void main()
{
	vec4 RelativePosition = Draws[gl_InstanceIndex].LocalToRelative * vec4(InPosition, 1.0);
	gl_Position = View.ShadowMatrices[ShadowPass.MatrixIndex] * RelativePosition;
}