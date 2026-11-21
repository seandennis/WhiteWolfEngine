// How much of a directional light reaches a surface: whether something stands between the two.
// Needs SceneData.glsl.
//
// Each light that casts shadows has drawn the depth of what it sees into shadow maps, one for each of four
// slices of the camera's view (see FShadowCascades in C++). A surface point is in shadow where the map holds
// something nearer the light than the point is. Many comparisons around the point are averaged, which makes the
// edge of a shadow soft instead of jagged.

// How many shadow map pixels to each side the comparisons reach: a square of five by five.
const int ShadowKernelRadius = 2;

// Returns the share of the light that one shadow map lets through to a position, from 0 to 1.
float ReadShadowMap(int FirstLayer, int Cascade, vec3 RelativePosition)
{
	int Layer = FirstLayer + Cascade;
	vec4 InMap = View.ShadowMatrices[Layer] * vec4(RelativePosition, 1.0);

	// Across the map, y runs upwards; down its rows, the texture coordinate runs downwards.
	vec2 Coordinate = vec2(InMap.x * 0.5 + 0.5, 0.5 - InMap.y * 0.5);

	// A surface must not shadow itself because of the map's limited precision.
	float Depth = InMap.z - View.ShadowCascadeScales[Cascade].y;

	float PixelSize = View.ShadowParameters.x;
	float Sum = 0.0;
	for (int Y = -ShadowKernelRadius; Y <= ShadowKernelRadius; ++Y)
	{
		for (int X = -ShadowKernelRadius; X <= ShadowKernelRadius; ++X)
		{
			vec2 Offset = vec2(float(X), float(Y)) * PixelSize;
			Sum += texture(sampler2DArrayShadow(ShadowMaps, ShadowSampler), vec4(Coordinate + Offset, float(Layer), Depth));
		}
	}

	float SampleCount = float((2 * ShadowKernelRadius + 1) * (2 * ShadowKernelRadius + 1));
	return Sum / SampleCount;
}

// Returns the share of a directional light that reaches a position, from 0, in shadow, to 1, fully lit.
float GetShadowVisibility(int LightIndex, vec3 RelativePosition)
{
	int FirstLayer = int(View.DirectionalLights[LightIndex].DirectionToLight.w);
	if (FirstLayer < 0)
	{
		return 1.0;
	}

	// How far the position is from the camera along the way the camera looks decides which cascade it is in.
	float Distance = dot(RelativePosition, View.CameraForward.xyz);
	float ShadowDistance = View.CameraForward.w;
	if (Distance >= ShadowDistance)
	{
		return 1.0;
	}

	int Cascade = 0;
	for (int Index = 0; Index < ShadowCascadeCount - 1; ++Index)
	{
		if (Distance > View.ShadowCascadeFarDistances[Index])
		{
			Cascade = Index + 1;
		}
	}

	float Visibility = ReadShadowMap(FirstLayer, Cascade, RelativePosition);

	// Towards the end of a cascade its answer gives way to the next one's, so that no line shows where one map
	// takes over from another; the last cascade gives way to no shadow at all.
	float CascadeFar = View.ShadowCascadeFarDistances[Cascade];
	float BlendStart = CascadeFar * 0.9;
	if (Distance > BlendStart)
	{
		float NextVisibility = Cascade < ShadowCascadeCount - 1 ? ReadShadowMap(FirstLayer, Cascade + 1, RelativePosition) : 1.0;
		Visibility = mix(Visibility, NextVisibility, (Distance - BlendStart) / (CascadeFar - BlendStart));
	}

	return Visibility;
}