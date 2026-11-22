// What the shaders of user interfaces are told with each run of triangles. The same structure is FUiConstants in
// Engine/Private/Rendering/UiRenderer.cpp; the two must stay identical.

layout(push_constant) uniform FUiConstants
{
	// A position in pixels, multiplied by Scale and with Translate added, is a position from -1 to 1 across the
	// surface, with the top left corner of the surface at (-1, 1).
	vec2 Scale;
	vec2 Translate;

	// 1 to use the alpha of the picture, 0 to take the picture as covering everything.
	float TextureAlpha;
} Ui;
