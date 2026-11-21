// Bends the direction a surface faces by what a normal texture says, which gives a flat triangle the lighting of
// a detailed surface.
//
// A normal texture stores directions relative to the surface: red is along the texture's first coordinate, green
// is up in the picture (against its second coordinate, which runs downwards) and blue is away from the surface,
// as glTF defines it. Which way those are in the world is worked out here from how the position and the texture
// coordinate change from one pixel to the next, so meshes need no tangents.
//
// Normal: the unit vector the surface faces along, unbent.
// Position: where the surface point is; any space with the same axes as Normal.
// Sample: what the normal texture holds at the point, as read.
// Scale: how strongly the texture bends the normal; 1 is as authored.
vec3 ApplyNormalTexture(vec3 Normal, vec3 Position, vec2 TextureCoordinate, vec3 Sample, float Scale)
{
	vec3 PositionAcross = dFdx(Position);
	vec3 PositionDown = dFdy(Position);
	vec2 CoordinateAcross = dFdx(TextureCoordinate);
	vec2 CoordinateDown = dFdy(TextureCoordinate);

	// How the position changes with each texture coordinate.
	float Determinant = CoordinateAcross.x * CoordinateDown.y - CoordinateDown.x * CoordinateAcross.y;
	if (abs(Determinant) < 1.0e-12)
	{
		// The texture is not laid out over this surface at all, so it cannot say anything about it.
		return Normal;
	}
	vec3 AlongFirst = (CoordinateDown.y * PositionAcross - CoordinateAcross.y * PositionDown) / Determinant;
	vec3 AlongSecond = (CoordinateAcross.x * PositionDown - CoordinateDown.x * PositionAcross) / Determinant;

	vec3 Tangent = AlongFirst - Normal * dot(Normal, AlongFirst);
	if (dot(Tangent, Tangent) < 1.0e-20)
	{
		return Normal;
	}
	Tangent = normalize(Tangent);

	// "Up in the picture" is against the second texture coordinate. Taking its side from the mesh keeps the
	// texture right on surfaces where it is laid out mirrored.
	vec3 Bitangent = cross(Normal, Tangent);
	if (dot(Bitangent, AlongSecond) > 0.0)
	{
		Bitangent = -Bitangent;
	}

	vec3 Direction = Sample * 2.0 - 1.0;
	Direction.xy *= Scale;
	return normalize(Tangent * Direction.x + Bitangent * Direction.y + Normal * Direction.z);
}