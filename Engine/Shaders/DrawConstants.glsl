// What changes from one drawn mesh to the next. The renderer sets these before each draw.
// The layout must match FDrawConstants in Engine/Private/Rendering/DrawConstants.h.
layout(push_constant) uniform FDrawConstants
{
	// Takes a position in the mesh's own space to clip space. It is built relative to the camera,
	// so no number in it grows with the distance from the world origin.
	mat4 LocalToClip;

	// The base colour of the material, in linear space.
	vec4 BaseColor;
} Draw;