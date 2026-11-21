// What the ambient occlusion passes read: the depth of the picture and the way its surfaces face, as the prepass
// drew them, and how to turn a pixel and its depth back into a position.

layout(set = 0, binding = 0) uniform texture2D SceneDepth;
layout(set = 0, binding = 1) uniform texture2D SceneNormals;

layout(push_constant) uniform FAmbientOcclusionConstants
{
	// Turns a camera-relative direction into the camera's own space, where it looks along -Z.
	mat4 RelativeToView;

	// x: how far to the side the edge of the picture is per metre of distance, y: the same upwards, z: the near
	// plane, w: how many pixels a metre covers at a distance of one metre.
	vec4 Projection;

	// x: the radius within which surfaces occlude each other, in metres. y: the strength of the effect.
	// z, w: the width and height of the picture in pixels.
	vec4 Settings;

	// x, y: the direction the blur pass works in, in pixels: (1, 0) or (0, 1).
	vec4 Blur;
} Pass;

// Returns the depth the prepass drew at a pixel: the near plane divided by the distance, and zero where there is
// nothing but sky.
float ReadDepth(ivec2 Pixel)
{
	return texelFetch(SceneDepth, Pixel, 0).r;
}

// Returns where the surface at a pixel is, in the camera's own space, given its depth.
vec3 GetViewPosition(ivec2 Pixel, float Depth)
{
	// Across the picture x runs from -1 to 1 rightwards, and y from 1 at the top row down to -1.
	vec2 Place = (vec2(Pixel) + 0.5) / Pass.Settings.zw * 2.0 - 1.0;
	float Distance = Pass.Projection.z / Depth;
	return vec3(Place.x * Pass.Projection.x * Distance, -Place.y * Pass.Projection.y * Distance, -Distance);
}