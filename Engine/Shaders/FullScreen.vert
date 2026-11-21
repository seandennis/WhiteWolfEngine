#version 460

// Covers the whole picture with one triangle, for passes that work on every pixel. It needs no vertex buffer:
// the three corners follow from the vertex number.

void main()
{
	vec2 Corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(Corner * 2.0 - 1.0, 0.0, 1.0);
}