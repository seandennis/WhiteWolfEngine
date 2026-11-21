// How a surface reflects light: the metallic-roughness model of glTF 2.0 (its appendix B), with the GGX
// distribution of microfacets, the height-correlated Smith visibility term and Schlick's Fresnel approximation.

const float Pi = 3.14159265358979;

// Below this a highlight becomes smaller than a pixel and the arithmetic of the model breaks down.
const float MinRoughness = 0.045;

// A surface point, as the reflection model sees it.
struct FSurface
{
	// What a matte surface of this material reflects. Black for a metal.
	vec3 DiffuseColor;

	// What the surface reflects when it is looked at head-on.
	vec3 SpecularColor;

	// The square of the roughness, which is what the model works with.
	float Alpha;

	// The unit vector the surface faces along.
	vec3 Normal;

	// The unit vector from the surface towards the camera.
	vec3 ToCamera;
};

FSurface MakeSurface(vec3 BaseColor, float Metallic, float Roughness, vec3 Normal, vec3 ToCamera)
{
	float ClampedMetallic = clamp(Metallic, 0.0, 1.0);
	float ClampedRoughness = clamp(Roughness, MinRoughness, 1.0);

	FSurface Surface;
	Surface.DiffuseColor = BaseColor * (1.0 - ClampedMetallic);
	// A material that is not a metal reflects four percent of the light, uncoloured; a metal reflects its colour.
	Surface.SpecularColor = mix(vec3(0.04), BaseColor, ClampedMetallic);
	Surface.Alpha = ClampedRoughness * ClampedRoughness;
	Surface.Normal = Normal;
	Surface.ToCamera = ToCamera;
	return Surface;
}

// Returns the light that leaves the surface towards the camera, given the light that arrives from one direction.
// Illuminance is what a surface facing the light would receive.
vec3 ReflectLight(FSurface Surface, vec3 ToLight, vec3 Illuminance)
{
	float NormalDotLight = dot(Surface.Normal, ToLight);
	if (NormalDotLight <= 0.0)
	{
		// The light is behind the surface.
		return vec3(0.0);
	}

	// A camera that grazes the surface, or sees it from behind through an interpolated normal, would divide by
	// zero below.
	float NormalDotCamera = max(dot(Surface.Normal, Surface.ToCamera), 1.0e-4);
	vec3 Halfway = normalize(ToLight + Surface.ToCamera);
	float NormalDotHalfway = clamp(dot(Surface.Normal, Halfway), 0.0, 1.0);
	float CameraDotHalfway = clamp(dot(Surface.ToCamera, Halfway), 0.0, 1.0);

	float AlphaSquared = Surface.Alpha * Surface.Alpha;

	// How many microfacets face halfway between the light and the camera.
	float Denominator = NormalDotHalfway * NormalDotHalfway * (AlphaSquared - 1.0) + 1.0;
	float Distribution = AlphaSquared / (Pi * Denominator * Denominator);

	// How many of them are neither in shadow nor hidden from the camera.
	float LightTerm = NormalDotCamera * sqrt(NormalDotLight * NormalDotLight * (1.0 - AlphaSquared) + AlphaSquared);
	float CameraTerm = NormalDotLight * sqrt(NormalDotCamera * NormalDotCamera * (1.0 - AlphaSquared) + AlphaSquared);
	float Visibility = 0.5 / (LightTerm + CameraTerm);

	// How much of the light is reflected rather than entering the surface.
	vec3 Fresnel = Surface.SpecularColor + (1.0 - Surface.SpecularColor) * pow(1.0 - CameraDotHalfway, 5.0);

	vec3 Specular = Fresnel * (Distribution * Visibility);
	vec3 Diffuse = (1.0 - Fresnel) * Surface.DiffuseColor / Pi;
	return (Diffuse + Specular) * Illuminance * NormalDotLight;
}