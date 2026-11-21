// Fits the brightness of a scene, which has no upper limit, into the range a display can show.
//
// This is the Khronos PBR Neutral tone mapper. Colours of moderate brightness pass through almost unchanged, so
// that a material looks the way it was authored; only bright highlights are compressed, and they fade towards
// white instead of clipping.
vec3 ToneMap(vec3 Color)
{
	const float StartCompression = 0.8 - 0.04;
	const float Desaturation = 0.15;

	// Keep the arithmetic below within what floating point can do, whatever the exposure made of the light.
	Color = clamp(Color, vec3(0.0), vec3(1.0e6));

	// Take off the four percent that every non-metal reflects, so that base colours come out as they went in.
	float Smallest = min(Color.r, min(Color.g, Color.b));
	float Offset = Smallest < 0.08 ? Smallest - 6.25 * Smallest * Smallest : 0.04;
	Color -= Offset;

	float Peak = max(Color.r, max(Color.g, Color.b));
	if (Peak < StartCompression)
	{
		return Color;
	}

	// Compress everything above the threshold into what is left of the range.
	float Headroom = 1.0 - StartCompression;
	float NewPeak = 1.0 - Headroom * Headroom / (Peak + Headroom - StartCompression);
	Color *= NewPeak / Peak;

	// The brighter the highlight, the closer to white it becomes.
	float Whiteness = 1.0 - 1.0 / (Desaturation * (Peak - NewPeak) + 1.0);
	return mix(Color, vec3(NewPeak), Whiteness);
}