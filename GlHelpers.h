#pragma once
#include "PrecompiledHeaders.hpp"

#include <GL/glew.h>
#include <assimp/vector3.h>
#include <assimp/vector2.h>
#include <assimp/color4.h>
#define GLSL_VERSION 430

namespace GlHelpers {
	void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
	void initCallback();
	void setVertexAttrib(GLuint vao, GLuint attrib, GLint size, GLenum type, GLuint buffer, GLintptr offset, GLsizei stride);
	void linkProgram(GLuint program);
	template<GLenum SHADER_TYPE>
	bool compileShader(std::string filename, GLuint& shader, std::initializer_list<std::string> defines)
	{
		std::cout << "Compiling shader " << filename << std::endl;
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
	glm::vec3 aiToGlm(aiVector3D vec);
	glm::vec2 aiToGlm(aiVector2D vec);
	glm::vec4 aiToGlm(aiColor4D vec);
	template<typename O, typename I>
	O structConvert(I in)
	{
		return O(in.x, in.y, in.z);
	}
};