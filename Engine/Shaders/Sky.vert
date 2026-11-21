#version 460

// Covers the whole picture with one triangle at the farthest possible depth and works out, for each corner, the
// direction the camera looks in there. The sky is drawn with it wherever no surface is.

#include "SceneData.glsl"

layout(location = 0) out vec3 OutDirection;

void main()
{
	vec2 Corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vec2 Position = Corner * 2.0 - 1.0;

	// Depth is reversed: zero is infinitely far away.
	gl_Position = vec4(Position, 0.0, 1.0);
	OutDirection = (View.ClipToDirection * vec4(Position, 1.0, 0.0)).xyz;
}