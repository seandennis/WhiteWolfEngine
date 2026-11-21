#version 460

// Places the vertices of a static mesh on the screen.

#include "DrawConstants.glsl"

layout(location = 0) in vec3 InPosition;

void main()
{
	gl_Position = Draw.LocalToClip * vec4(InPosition, 1.0);
}