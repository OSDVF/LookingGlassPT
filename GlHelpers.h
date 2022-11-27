#include <gl/glew.h>
#define GLSL_VERSION 420

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
	static bool compileShader(std::string filename, GLuint& shader, std::initializer_list<std::string> defines)
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
		std::string versionString = "#version " + std::to_string(GLSL_VERSION) + "\n";
		std::vector<const GLchar*> shaderParts({
			versionString.c_str()
			});
		for (auto& define : defines)
		{
			shaderParts.emplace_back("#define ");
			shaderParts.emplace_back(define.c_str());
			shaderParts.emplace_back("\n");
		}
		shaderParts.push_back(buffer.c_str());
		glShaderSource(shader, shaderParts.size(), shaderParts.data(), nullptr); // Let OpenGL read until null terminator
		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
		{
			std::cerr << "GL Error: " << error << std::endl;
		}
		glCompileShader(shader);
		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			std::cerr << "Shader " << filename << " compilation failed" << std::endl;
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

	static void linkProgram(GLuint program)
	{
		glLinkProgram(program);
		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			std::string infoLog(512, ' ');
			glGetProgramInfoLog(success, 512, NULL, (GLchar*)infoLog.c_str());
			std::cerr << "Shader link failed:\n" << infoLog << std::endl;
		}
	}
};