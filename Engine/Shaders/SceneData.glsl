// What the renderer gives the shaders that draw a scene. Each structure here must match the C++ structure of the
// same name in Engine/Private/Rendering, member for member.
//
// Positions and directions are "camera-relative": measured from the camera's location along the world's axes.
// Nothing here grows with the distance from the world origin.

// One drawn mesh. A shader finds its own with gl_InstanceIndex.
struct FDrawData
{
	// Takes a position in the mesh's own space to clip space.
	mat4 LocalToClip;

	// Takes a position in the mesh's own space to camera-relative space.
	mat4 LocalToRelative;

	// Takes a normal in the mesh's own space to camera-relative space. The result is not of unit length.
	mat4 NormalToRelative;

	// The base colour of the material, in linear space.
	vec4 BaseColor;

	// The light the surface gives off by itself.
	vec4 EmissiveColor;

	// x: metallic, y: roughness.
	vec4 MetallicRoughness;

	// x: how strongly the normal texture bends the surface's normal. y: 1 if the material has a normal texture.
	// z: how strongly the occlusion texture dims the light of the surroundings.
	vec4 NormalScale;
};

// A light whose rays are parallel.
struct FDirectionalLightData
{
	// xyz: the unit vector from a surface towards the light.
	vec4 DirectionToLight;

	// rgb: the light arriving at a surface that faces the light, colour times illuminance.
	vec4 Illuminance;
};

// A light that shines in all directions from a point.
struct FPointLightData
{
	// xyz: where the light is, camera-relative. w: the distance at which it stops, or zero for no limit.
	vec4 LocationAndRange;

	// rgb: colour times luminous intensity.
	vec4 Intensity;
};

// The most light the buffer a scene is lit in can hold: just under the largest 16-bit floating-point number.
// Anything brighter is this bright. The same as MaxSceneLight in Engine/Private/Rendering/SceneRenderer.cpp.
const float MaxSceneLight = 65000.0;

const int MaxDirectionalLights = 4;
const int MaxPointLights = 32;

layout(set = 0, binding = 0, std430) readonly buffer FDrawBuffer
{
	FDrawData Draws[];
};

// Constant buffers are bound from 256 on; see the binding offsets of NVRHI's Vulkan backend.
layout(set = 0, binding = 256, std140) uniform FViewConstants
{
	// x: the number of directional lights, y: the number of point lights.
	ivec4 LightCounts;

	FDirectionalLightData DirectionalLights[MaxDirectionalLights];
	FPointLightData PointLights[MaxPointLights];

	// The light a matte surface gathers from the sky light's environment, for every direction it can face, as
	// spherical harmonics, already multiplied by the sky light's intensity. See Environment.glsl.
	vec4 SkyIrradiance[9];

	// Turns a camera-relative direction into the direction in the environment's picture: the inverse of the sky
	// light's rotation.
	mat4 RelativeToEnvironment;

	// Gives the camera-relative direction the camera looks in at a place on the screen: multiply it with
	// (x, y, 1, 0), where x and y run from -1 to 1 across the picture.
	mat4 ClipToDirection;

	// x: the intensity of the sky light, zero if the scene has none. y: the number of the last, roughest level of
	// the environment's reflection cube map. w: the level of detail the sky is read at.
	vec4 SkyParameters;
} View;

// The environment of the sky light: blurred by roughness for reflections, and as it is for the sky. Textures are
// bound from 0 on and samplers from 128 on; see the binding offsets of NVRHI's Vulkan backend.
layout(set = 0, binding = 1) uniform textureCube EnvironmentReflections;
layout(set = 0, binding = 2) uniform texture2D EnvironmentPicture;
layout(set = 0, binding = 128) uniform sampler EnvironmentReflectionsSampler;
layout(set = 0, binding = 129) uniform sampler EnvironmentPictureSampler;