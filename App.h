#pragma once
#include <SDL.h>
#include "UsbCalibration.h"
#include "BridgeCalibration.h"
#include <fstream>
#include <sstream>
#include <array>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "GlHelpers.h"
#include "FirstPersonController.h"
#include "Helpers.h"

struct {
	GLint uTime;
	GLint uWindowSize;
	GLint uWindowPos;
	GLint uMouse;
	GLint uView;
	GLint uProj;
} uniforms;

class App {
public:
	static inline GLuint fShader;
	static inline GLuint fFlatShader;
	static inline GLuint vShader;
	static inline GLuint program;
	static inline Calibration calibration;
	static inline GLuint fullScreenVAO;
	static inline GLuint fullScreenVertexBuffer;
	static inline float windowWidth;
	static inline float windowHeight;
	static inline float fov = 60;
	static inline float farPlane = 1000;
	static inline float nearPlane = 0.1;
	static inline FirstPersonController person;
	static inline long lastTime;
	static inline enum class ScreenType {
		Flat = 0, LookingGlass = 1
	} ScreenType;
	static void setup(ImGuiIO& io, int x, int y, int w, int h, bool forceFlat = false)
	{
		GlHelpers::initCallback();
		ScreenType = ScreenType::Flat;
		person.Camera.Sensitivity = 0.000001f;
		windowWidth = w;
		windowHeight = h;
		if (forceFlat)
		{
			std::cout << "Forced flat screen." << std::endl;
		}
		else
		{
			try {
				try {
					std::cout << "Trying Looking Glass Bridge calibration..." << std::endl;
					calibration = BridgeCalibration().getCalibration();
				}
				catch (const std::runtime_error& e)
				{
					std::cerr << e.what() << std::endl;
					std::cout << "Trying USB calibration..." << std::endl;
					calibration = UsbCalibration().getCalibration();
				}
				std::cout << "Calibration success: " << std::endl << calibration;
				ScreenType = ScreenType::LookingGlass;
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
				std::cerr << "Calibration failed. Using default values." << std::endl;
			}
		}
		program = glCreateProgram();
		GlHelpers::compileShader<GL_VERTEX_SHADER>(Helpers::relativeToExecutable("vertex.vert").string(), vShader, {});
		glAttachShader(program, vShader);
		recompileFragmentSh();
		GlHelpers::linkProgram(program);
		glCreateVertexArrays(1, &fullScreenVAO);
		// Assign to fullScreenVertexBuffer
		glCreateBuffers(1, &fullScreenVertexBuffer);
		std::array<glm::vec2, 4> vertices = {
			glm::vec2{-1.0f,-1.f},
			{1.f,-1.f},
			{-1.f,1.f},
			{1.f,1.f}
		};
		// Transfer data to fullScreenVertexBuffer
		glNamedBufferData(fullScreenVertexBuffer, vertices.size() * sizeof(glm::vec2), vertices.data(), GL_STATIC_DRAW);
		GlHelpers::setVertexAttrib(fullScreenVAO, 0, 2, GL_FLOAT, fullScreenVertexBuffer, 0, sizeof(glm::vec2));

		GLuint calibrationBlockI = glGetUniformBlockIndex(program, "Calibration");
		GLint blockSize;
		glGetActiveUniformBlockiv(program, calibrationBlockI, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

		auto calibrationForShader = calibration.forShader();
		if (blockSize != sizeof(calibrationForShader))
		{
			std::cerr << "Bad shader memory layout. Calibration block is " << blockSize << " bytes, but should be " << sizeof(calibrationForShader) << std::endl;
		}

		/*const GLchar* names[] = {"pitch", "slope","center"};
		GLuint indices[3];
		glGetUniformIndices(program, 3, names, indices);

		GLint offset[3];
		glGetActiveUniformsiv(program, 3, indices, GL_UNIFORM_OFFSET, offset);
		*/
		GLuint uboHandle;
		glGenBuffers(1, &uboHandle);
		glBindBuffer(GL_UNIFORM_BUFFER, uboHandle);
		glBufferData(GL_UNIFORM_BUFFER, blockSize, (const void*)&calibrationForShader, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboHandle);

		bindUniforms();
		glBindVertexArray(fullScreenVAO);
		glUseProgram(program);
		glUniform1f(uniforms.uTime, 0);
		glUniform2f(uniforms.uWindowSize, w, h);
		glUniform2f(uniforms.uWindowPos, x, y);
		glUniform2f(uniforms.uMouse, 0.5f, 0.5f);

		person.Camera.SetProjectionMatrixPerspective(fov, (float)w / h, nearPlane, farPlane);
		person.Camera.SetCenter(glm::vec2(w / 2, h / 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		lastTime = SDL_GetPerformanceCounter();
	}
	static void bindUniforms()
	{
		uniforms = {
			glGetUniformLocation(program, "uTime"),
			glGetUniformLocation(program, "uWindowSize"),
			glGetUniformLocation(program, "uWindowPos"),
			glGetUniformLocation(program, "uMouse"),
			glGetUniformLocation(program, "uView"),
			glGetUniformLocation(program, "uProj"),
		};
	}

	static void recompileFragmentSh()
	{
		// Will produce errors if the shader is not compiled yet but c'est la vie
		GLsizei count;
		GLuint shaders[5];
		glGetAttachedShaders(program, sizeof(shaders) / sizeof(GLuint), &count, shaders);
		for (int i = 0; i < count; i++)
		{
			GLint shaderType;
			glGetShaderiv(shaders[i], GL_SHADER_TYPE, &shaderType);
			if (shaderType == GL_FRAGMENT_SHADER)
			{
				glDetachShader(program, shaders[i]);
			}
		}

		std::string fragSource = Helpers::relativeToExecutable("fragment.frag").string();
		GlHelpers::compileShader<GL_FRAGMENT_SHADER>(fragSource, fShader, {});
		GlHelpers::compileShader<GL_FRAGMENT_SHADER>(fragSource, fFlatShader, { "FLAT_SCREEN" });
		glAttachShader(program, ScreenType == ScreenType::Flat ? fFlatShader : fShader);
	}

	static inline float f;
	static inline int counter = 0;
	static inline float frame = 1;
	static inline bool interactive = false;
	static void draw(ImGuiIO& io, SDL_Event event)
	{
		auto now = SDL_GetPerformanceCounter();
		float deltaTime = (now - lastTime) / (float)SDL_GetPerformanceFrequency();
		if (interactive)
		{
			person.Update(deltaTime, false, event);
		}
		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED:
				glUniform2f(uniforms.uWindowSize, event.window.data1, event.window.data2);
				windowWidth = event.window.data1;
				windowHeight = event.window.data2;
				person.Camera.SetProjectionMatrixPerspective(fov, windowWidth / windowHeight, nearPlane, farPlane);
				//std::cout << "W:" << event.window.data1 << "H:" << event.window.data2 << std::endl;
				break;
			case SDL_WINDOWEVENT_MOVED:
				glUniform2f(uniforms.uWindowPos, event.window.data1, event.window.data2);
				//std::cout << "X:" << event.window.data1 << "Y:" << event.window.data2 << std::endl;
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			if (interactive)
			{
				glUniform2f(uniforms.uMouse, event.motion.x, event.motion.y);
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.sym)
			{
			case SDL_KeyCode::SDLK_r:
				recompileFragmentSh();
				GlHelpers::linkProgram(program);
				break;
			case SDL_KeyCode::SDLK_l:
				if (ScreenType == ScreenType::Flat)
				{
					ScreenType = ScreenType::LookingGlass;
					swapShaders(fFlatShader, fShader);
				}
				else
				{
					ScreenType = ScreenType::Flat;
					swapShaders(fShader, fFlatShader);
				}
				break;
			case SDL_KeyCode::SDLK_i:
				interactive = !interactive;
				break;
			}
			break;
		}
		glBindVertexArray(fullScreenVAO);
		glUseProgram(program);
		glUniform1f(uniforms.uTime, frame);
		glUniformMatrix4fv(uniforms.uView, 1, false, glm::value_ptr(person.Camera.GetViewMatrix()));
		glUniformMatrix4fv(uniforms.uProj, 1, false, glm::value_ptr(person.Camera.GetProjectionMatrix()));

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
		if (ImGui::RadioButton("Looking Glass", (int*)&ScreenType, (int)ScreenType::LookingGlass))
		{
			swapShaders(fFlatShader, fShader);
		}
		if (ImGui::RadioButton("Flat", (int*)&ScreenType, (int)ScreenType::Flat))
		{
			swapShaders(fShader, fFlatShader);
		}

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();

		// Draw full screen quad with the path tracer shader
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		frame++;
	}
	static void swapShaders(GLuint before, GLuint after)
	{
		glDetachShader(program, before);
		glAttachShader(program, after);
		GlHelpers::linkProgram(program);
		bindUniforms();
		glUniform2f(uniforms.uWindowSize, windowWidth, windowHeight);
	}
};