#version 460

// Places the vertices of a static mesh on the screen and hands the surface on to the fragment shader.

#include "SceneData.glsl"

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InTextureCoordinate;

// The prepass and the lit pass draw the same meshes and compare their depths, so both must arrive at exactly the
// same position.
invariant gl_Position;

layout(location = 0) out vec3 OutRelativePosition;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec2 OutTextureCoordinate;
layout(location = 3) flat out int OutDrawIndex;

void main()
{
	vec4 Position = vec4(InPosition, 1.0);
	gl_Position = Draws[gl_InstanceIndex].LocalToClip * Position;
	OutRelativePosition = (Draws[gl_InstanceIndex].LocalToRelative * Position).xyz;
	OutNormal = mat3(Draws[gl_InstanceIndex].NormalToRelative) * InNormal;
	OutTextureCoordinate = InTextureCoordinate;
	OutDrawIndex = gl_InstanceIndex;
}