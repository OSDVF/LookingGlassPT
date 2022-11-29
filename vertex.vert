in vec2 InVertexPos;
out vec2 vNDCpos;
void main()
{
	gl_Position = vec4(InVertexPos, 0.0f, 1.0f);
	vNDCpos = InVertexPos;
}