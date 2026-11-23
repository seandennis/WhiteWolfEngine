#version 460

// Marks where a mesh is in the picture, for the line that is drawn around it afterwards: one wherever one of its
// triangles is, whichever side it shows and whatever is in front of it.

layout(location = 0) out float OutMark;

void main()
{
	OutMark = 1.0;
}
