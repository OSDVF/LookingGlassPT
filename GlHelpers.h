#include <gl/glew.h>

class GlHelpers {
public:
	static void setVertexAttrib(
		GLuint   vao,
		GLuint   attrib,
		GLint    size,
		GLenum   type,
		GLuint   buffer,
		GLintptr offset,
		GLsizei  stride) {
		glVertexArrayAttribBinding(
			vao,
			attrib, //attrib index
			attrib);//binding index

		glEnableVertexArrayAttrib(
			vao,
			attrib); // attrib index

		glVertexArrayAttribFormat(
			vao,
			attrib, //attrib index
			size,
			type,
			GL_FALSE, //normalization
			0);//relative offset

		glVertexArrayVertexBuffer(
			vao,
			attrib, //binding index
			buffer,
			offset,
			stride);
	}

	template<GLenum SHADER_TYPE>
	static bool compileShader(std::string filename, GLuint& shader)
	{
		shader = glCreateShader(SHADER_TYPE);
		std::ifstream t;
		t.open(filename);
		t.seekg(0, std::ios::end);
		GLint size = t.tellg();
		if (size <= 0)
		{
			throw std::runtime_error(std::format("Shader file {} not found or empty.", filename));
		}
		std::string buffer(size, ' ');
		t.seekg(0);
		t.read(&buffer[0], size);
		auto pointerToShaderSource = buffer.c_str();

		glShaderSource(shader, 1, (const GLchar* const*)&pointerToShaderSource, &size);
		glCompileShader(shader);
		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			std::cerr << "Shader compilation failed";
			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
			maxLength = std::max(maxLength, 512);

			// The maxLength includes the NULL character
			GLchar* errorLog = new GLchar[maxLength];
			glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog);
			std::cerr << errorLog;
			std::cerr << std::endl;
			delete[] errorLog;

			// Provide the infolog in whatever manor you deem best.
			// Exit with failure.
			glDeleteShader(shader); // Don't leak the shader.
			return false;
		}
		return true;
	}
};