#include "GlHelpers.h"
#include <boost/stacktrace.hpp>

namespace GlHelpers {
	void GLAPIENTRY
		MessageCallback(GLenum source,
			GLenum type,
			GLuint id,
			GLenum severity,
			GLsizei length,
			const GLchar* message,
			const void* userParam)
	{
		switch (type) {
		case GL_DEBUG_TYPE_ERROR:
			std::cerr << "ERROR";
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			std::cerr << "DEPRECATED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			std::cerr << "UNDEFINED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			std::cerr << "PORTABILITY";
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			std::cerr << "PERFORMANCE";
			break;
		case GL_DEBUG_TYPE_OTHER:
			std::cerr << "OTHER";
			break;
		}
		std::cerr << " (severity ";
		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			std::cerr << "NOTIFICATION";
			break;
		case GL_DEBUG_SEVERITY_LOW:
			std::cerr << "LOW";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			std::cerr << "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_HIGH:
			std::cerr << "HIGH";
			break;
		}
		
		std::cerr << "): " << message << std::endl;
#ifdef _DEBUG
		if (type == GL_DEBUG_TYPE_ERROR)
		{
			std::cerr << boost::stacktrace::stacktrace() << std::endl;
		}
#endif
	}

	void initCallback()
	{
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(MessageCallback, 0);
	}

	void setVertexAttrib(
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

	void linkProgram(GLuint program)
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

	glm::vec3 aiToGlm(aiVector3D vec)
	{
		return glm::vec3(vec.x, vec.y, vec.z);
	}

	glm::vec2 aiToGlm(aiVector2D vec)
	{
		return glm::vec2(vec.x, vec.y);
	}

	glm::vec4 aiToGlm(aiColor4D vec)
	{
		return glm::vec4(vec.r, vec.g, vec.b, vec.a);
	}
};