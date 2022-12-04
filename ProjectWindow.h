#pragma once
#include <SDL.h>
#include "UsbCalibration.h"
#include "BridgeCalibration.h"
#include <fstream>
#include <sstream>
#include <array>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "GlHelpers.h"
#include "Helpers.h"
#include "Settings.h"
#include "SceneObjects.h"

struct {
	GLint uTime;
	GLint uWindowSize;
	GLint uWindowPos;
	GLint uMouse;
	GLint uView;
	GLint uProj;
	GLint uViewCone;
	GLint uFocusDistance;
} uniforms;

using namespace ProjectSettings;
class ProjectWindow : public AppWindow {
public:
	GLuint fShader;
	GLuint fFlatShader;
	GLuint vShader;
	GLuint program;
	Calibration calibration;
	GLuint fullScreenVAO;
	GLuint fullScreenVertexBuffer;
	GLuint uCalibrationHandle;
	struct {
		GLuint vertex;
		GLuint index;
		GLuint objects;
		GLuint material;
		GLuint textures;
	} bufferHandles;

	std::vector<glm::vec3> vertices = {
		glm::vec3(0.5, 1, 1),
		{1, 0, 1},
		{0, 0, 1}
	};
	std::vector<unsigned int> indices = {
		0, 1, 2
	};

	anyVector objects = {
		SceneObject<1>{
			0, 0, 1, {3}
		}
	};

	anyVector materials = {
		Material<color, color, color, color, color>{
			{1.f,1.f,1.f},
			{1.f,1.f,1.f},
			{1.f,1.f,1.f},
			{1.f,1.f,1.f},
			{1.f,1.f,1.f}
	}
	};

	std::vector<GLuint64> textures;

	ProjectWindow(const char* name, float x, float y, float w, float h, bool forceFlat = false)
		: AppWindow(name, x, y, w, h)
	{
		eventDriven = false;

		std::cout << "Pixel scale: " << this->pixelScale << std::endl;
		if (forceFlat)
		{
			std::cout << "Forced flat screen." << std::endl;
			GlobalScreenType = ScreenType::Flat;
		}
		else
		{
			extractCalibration();
		}
	}

	// Runs on the render thread
	void setupGL() override
	{
		AppWindow::setupGL();
		GlHelpers::initCallback();

		program = glCreateProgram();
		GlHelpers::compileShader<GL_VERTEX_SHADER>(Helpers::relativeToExecutable("vertex.vert").string(), vShader, {});
		glAttachShader(program, vShader);
		recompileFragmentSh();
		GlHelpers::linkProgram(program);
		glCreateVertexArrays(1, &fullScreenVAO);
		// Assign to fullScreenVertexBuffer
		glCreateBuffers(1, &fullScreenVertexBuffer);
		std::array<glm::vec2, 4> screenQuadVertices = {
			glm::vec2{-1.0f,-1.f},
			{1.f,-1.f},
			{-1.f,1.f},
			{1.f,1.f}
		};
		// Transfer data to fullScreenVertexBuffer
		glNamedBufferData(fullScreenVertexBuffer, screenQuadVertices.size() * sizeof(glm::vec2), screenQuadVertices.data(), GL_STATIC_DRAW);
		GlHelpers::setVertexAttrib(fullScreenVAO, 0, 2, GL_FLOAT, fullScreenVertexBuffer, 0, sizeof(glm::vec2));

		GLuint calibrationBlockI = glGetUniformBlockIndex(program, "CalibrationBuffer");
		GLint blockSize;
		glGetActiveUniformBlockiv(program, calibrationBlockI, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

		auto calibrationForShader = calibration.forShader();
		if (blockSize != sizeof(calibrationForShader))
		{
			std::cerr << "Bad shader memory layout. Calibration block is " << blockSize << " bytes, but should be " << sizeof(calibrationForShader) << std::endl;
		}

		glGenBuffers(1, &uCalibrationHandle);
		glBindBuffer(GL_UNIFORM_BUFFER, uCalibrationHandle);
		glBufferData(GL_UNIFORM_BUFFER, blockSize, (const void*)&calibrationForShader, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, uCalibrationHandle);//For explanation: https://stackoverflow.com/questions/54955186/difference-between-glbindbuffer-and-glbindbufferbase

		glGenBuffers(1, &bufferHandles.vertex);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.vertex);
		glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufferHandles.vertex);

		glGenBuffers(1, &bufferHandles.index);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.index);
		glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufferHandles.index);

		bindDynamicBuffer(bufferHandles.objects, 3, objects);
		bindDynamicBuffer(bufferHandles.material, 4, materials);

		glGenBuffers(1, &bufferHandles.textures);
		glBindBuffer(GL_UNIFORM_BUFFER, bufferHandles.textures);
		glBufferData(GL_UNIFORM_BUFFER, textures.size() * sizeof(GLuint64), textures.data(), GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, bufferHandles.textures);

		bindUniforms();
		glBindVertexArray(fullScreenVAO);
		glUseProgram(program);
		glUniform1f(uniforms.uTime, 0);
		// These are one-time set uniforms
		glUniform2f(uniforms.uWindowSize, windowWidth, windowHeight);
		glUniform2f(uniforms.uWindowPos, windowPosX, windowPosY);
		glUniform2f(uniforms.uMouse, 0.5f, 0.5f);

		std::cout << objects.debug().rdbuf();
		std::cout << materials.debug().rdbuf();

		person.Camera.SetProjectionMatrixPerspective(fov, windowWidth / windowHeight, nearPlane, farPlane);
		person.Camera.SetCenter(glm::vec2(windowWidth / 2, windowHeight / 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	void bindDynamicBuffer(GLuint& bufferHandle, GLuint index, anyVector& buffer)
	{
		glGenBuffers(1, &bufferHandle);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandle);
		glBufferData(GL_SHADER_STORAGE_BUFFER, buffer.totalSize, buffer.buffer.view().data(), GL_STATIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, bufferHandle);
	}
	/**
	* Sets calibration and ScreenType
	*/
	void extractCalibration()
	{
		GlobalScreenType = ScreenType::Flat;
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
			GlobalScreenType = ScreenType::LookingGlass;
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			std::cerr << "Calibration failed. Using default values." << std::endl;
		}
	}

	void bindUniforms()
	{
		uniforms = {
			glGetUniformLocation(program, "uTime"),
			glGetUniformLocation(program, "uWindowSize"),
			glGetUniformLocation(program, "uWindowPos"),
			glGetUniformLocation(program, "uMouse"),
			glGetUniformLocation(program, "uView"),
			glGetUniformLocation(program, "uProj"),
			glGetUniformLocation(program, "uViewCone"),
			glGetUniformLocation(program, "uFocusDistance"),
		};
	}

	void recompileFragmentSh()
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
		glAttachShader(program, GlobalScreenType == ScreenType::Flat ? fFlatShader : fShader);
	}

	void render() override
	{
		if (ProjectSettings::changeScreenType)
		{
			swapScreens();
			ProjectSettings::changeScreenType = false;
		}
		if (ProjectSettings::recompileFShaders)
		{
			recompileFShaders = false;
			recompileFragmentSh();
			GlHelpers::linkProgram(program);
			bindUniforms();
			glUniform2f(uniforms.uWindowSize, windowWidth, windowHeight);
		}
		ImGui::Begin("Info");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
		glBindVertexArray(fullScreenVAO);
		glUseProgram(program);
		glUniform1f(uniforms.uTime, frame);
		glUniformMatrix4fv(uniforms.uView, 1, false, glm::value_ptr(person.Camera.GetViewMatrix()));
		glUniformMatrix4fv(uniforms.uProj, 1, false, glm::value_ptr(person.Camera.GetProjectionMatrix()));
		glUniform1f(uniforms.uViewCone, glm::radians(viewCone));
		glUniform1f(uniforms.uFocusDistance, focusDistance);
		glUniform2f(uniforms.uMouse, mouseX, mouseY);

		// Draw full screen quad with the path tracer shader
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		frame++;
	}

	float f;
	int counter = 0;
	float frame = 1;
	bool interactive = false;
	bool focal = false;
	bool fullscreen = false;

	void swapScreens()
	{
		if (GlobalScreenType == ScreenType::Flat)
		{
			GlobalScreenType = ScreenType::LookingGlass;
			swapShaders(fFlatShader, fShader);
		}
		else
		{
			GlobalScreenType = ScreenType::Flat;
			swapShaders(fShader, fFlatShader);
		}
	}

	void renderOnEvent(std::deque<SDL_Event>e) override
	{
		AppWindow::renderOnEvent(e);
		render();
	}

	bool workOnEvent(SDL_Event event, float deltaTime) override
	{
		if (interactive)
		{
			person.Update(deltaTime, false, event);
		}
		switch (event.type)
		{
		case SDL_KEYUP:
			switch (event.key.keysym.sym)
			{
			case SDL_KeyCode::SDLK_i:
				interactive = !interactive;
				break;
			case SDL_KeyCode::SDLK_m:
				focal = !focal;
				break;
			case SDLK_ESCAPE:
				return true;
			case SDLK_f:
				fullscreen = !fullscreen;
				if (fullscreen)
				{
					SDL_SetWindowFullscreen(window, flags | SDL_WINDOW_FULLSCREEN_DESKTOP);
				}
				else
				{
					SDL_SetWindowFullscreen(window, flags);
				}
				break;
			case SDLK_p:
				eventDriven = !eventDriven;
				break;
			case SDL_KeyCode::SDLK_r:
				ProjectSettings::recompileFShaders = true;
				break;
			case SDL_KeyCode::SDLK_l:
				ProjectSettings::changeScreenType = true;
				break;
			}
			break;
		case SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_CLOSE:
				hide();
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			if (focal)
			{
				mouseX = event.motion.x;
				mouseY = event.motion.y;
			}
			break;
		}
		return false;
	}
	void swapShaders(GLuint before, GLuint after)
	{
		glDetachShader(program, before);
		glAttachShader(program, after);
		GlHelpers::linkProgram(program);
		bindUniforms();
		glUniform2f(uniforms.uWindowSize, windowWidth, windowHeight);
	}

	void resized() override
	{
		AppWindow::resized();
		glUniform2f(uniforms.uWindowSize, windowWidth, windowHeight);
		person.Camera.SetProjectionMatrixPerspective(fov, windowWidth / windowHeight, nearPlane, farPlane);
	}
	void moved() override
	{
		AppWindow::moved();
		glUniform2f(uniforms.uWindowPos, windowPosX, windowPosY);
	}
};