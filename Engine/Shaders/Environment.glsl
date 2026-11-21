// The light of a whole surrounding, as WEnvironmentMap holds it: a picture that maps every direction to a place.
// The functions here must match WEnvironmentMap::GetCoordinateOfDirection and FSphericalHarmonics in C++.

const float EnvironmentPi = 3.14159265358979;

// Returns where in the picture a direction is: its middle is straight ahead (-Z), its right half to the right
// (+X), its top row up (+Y).
vec2 GetEnvironmentCoordinate(vec3 Direction)
{
	float Azimuth = atan(Direction.x, -Direction.z);
	float Polar = acos(clamp(Direction.y, -1.0, 1.0));
	return vec2(0.5 + Azimuth / (2.0 * EnvironmentPi), Polar / EnvironmentPi);
}

// Returns the value that nine spherical harmonics coefficients give for a unit direction.
vec3 EvaluateHarmonics(vec4 Coefficients[9], vec3 Direction)
{
	float X = Direction.x;
	float Y = Direction.y;
	float Z = Direction.z;
	return Coefficients[0].rgb * 0.282095 +
		Coefficients[1].rgb * (0.488603 * Y) +
		Coefficients[2].rgb * (0.488603 * Z) +
		Coefficients[3].rgb * (0.488603 * X) +
		Coefficients[4].rgb * (1.092548 * X * Y) +
		Coefficients[5].rgb * (1.092548 * Y * Z) +
		Coefficients[6].rgb * (0.315392 * (3.0 * Z * Z - 1.0)) +
		Coefficients[7].rgb * (1.092548 * X * Z) +
		Coefficients[8].rgb * (0.546274 * (X * X - Y * Y));
}