#version 460

// Draws the way each visible surface faces, before any lighting. Together with the depth that is drawn at the
// same time, this is what ambient occlusion is worked out from.

layout(location = 1) in vec3 InNormal;

layout(location = 0) out vec4 OutNormal;

void main()
{
	// A direction's parts run from -1 to 1; a colour's from 0 to 1.
	OutNormal = vec4(normalize(InNormal) * 0.5 + 0.5, 1.0);
}