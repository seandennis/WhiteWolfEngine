// The light a surface reflects from its whole surrounding, the environment of the scene's sky light.
// Needs SceneData.glsl, Environment.glsl and Brdf.glsl.

// Returns how much of the light of its surrounding a surface reflects towards the camera as a mirror image,
// summed over the whole surrounding. This is the lighting model of Brdf.glsl worked out ahead of time, in the
// approximate form Brian Karis gave for it ("Physically Based Shading on Mobile", 2014), which needs no table.
vec3 GetEnvironmentReflectance(vec3 SpecularColor, float Roughness, float NormalDotCamera)
{
	const vec4 Scale = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 Offset = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 Terms = Roughness * Scale + Offset;
	float Blend = min(Terms.x * Terms.x, exp2(-9.28 * NormalDotCamera)) * Terms.x + Terms.y;
	vec2 Factors = vec2(-1.04, 1.04) * Blend + Terms.zw;
	return SpecularColor * Factors.x + Factors.y;
}

// Returns the light a surface sends to the camera from the sky light's environment: what it gathers as a matte
// surface, and what it mirrors, more or less blurred by its roughness. Zero if the scene has no sky light.
vec3 ReflectEnvironment(FSurface Surface, float Roughness)
{
	float Intensity = View.SkyParameters.x;
	if (Intensity <= 0.0)
	{
		return vec3(0.0);
	}

	mat3 ToEnvironment = mat3(View.RelativeToEnvironment);
	float ClampedRoughness = clamp(Roughness, 0.0, 1.0);
	float NormalDotCamera = clamp(dot(Surface.Normal, Surface.ToCamera), 0.0, 1.0);
	vec3 Reflectance = GetEnvironmentReflectance(Surface.SpecularColor, ClampedRoughness, NormalDotCamera);

	// The harmonics can dip slightly below zero where a surrounding is dark; there is no negative light.
	vec3 Gathered = max(EvaluateHarmonics(View.SkyIrradiance, ToEnvironment * Surface.Normal), vec3(0.0));

	// Each level of the reflection cube map is the environment as a rougher surface mirrors it.
	vec3 MirrorDirection = ToEnvironment * reflect(-Surface.ToCamera, Surface.Normal);
	vec3 Mirrored = textureLod(samplerCube(EnvironmentReflections, EnvironmentReflectionsSampler),
		MirrorDirection,
		ClampedRoughness * View.SkyParameters.y).rgb * Intensity;

	return Surface.DiffuseColor * (1.0 - Reflectance) * Gathered / Pi + Mirrored * Reflectance;
}