#version 460

// Shows the environment of the scene's sky light behind everything else.

#include "SceneData.glsl"
#include "Environment.glsl"

layout(location = 0) in vec3 InDirection;

layout(location = 0) out vec4 OutColor;

void main()
{
	vec3 Direction = mat3(View.RelativeToEnvironment) * normalize(InDirection);
	vec3 Light = textureLod(sampler2D(EnvironmentPicture, EnvironmentPictureSampler),
		GetEnvironmentCoordinate(Direction),
		View.SkyParameters.w).rgb;
	OutColor = vec4(min(Light * View.SkyParameters.x, vec3(MaxSceneLight)), 1.0);
}