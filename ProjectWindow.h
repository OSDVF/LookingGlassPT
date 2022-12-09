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
#include <glm/gtx/quaternion.hpp>
#include "GlHelpers.h"
#include "Helpers.h"
#include "Settings.h"
#include "SceneObjects.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "alert_exception.h"

using namespace ProjectSettings;
class ProjectWindow : public AppWindow {
public:
	std::string calibrationAlert;
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
		GLuint triangles;
		GLuint objects;
		GLuint material;
		GLuint textures;
	} bufferHandles;
	struct BufferDefinition {
		GLuint index;
		GLint location;
	};
	struct {
		GLint uTime;
		GLint uWindowSize;
		GLint uWindowPos;
		GLint uMouse;
		GLint uView;
		GLint uProj;
		GLint uViewCone;
		GLint uFocusDistance;
		GLint uObjectCount;
		BufferDefinition uCalibration;
		BufferDefinition uObjects;
		BufferDefinition Attribute;
		BufferDefinition Triangles;
		BufferDefinition Material;
	} shaderInputs;

	anyVector vertexAttrs;
	std::vector<FastTriangle> triangles;
	std::vector<SceneObject> objects;
	anyVector materials;

	/*anyVector materials{
		Material<textureHandle, textureHandle, textureHandle, textureHandle, textureHandle>{
			0,1,2,3,4
	}
	};*/

	// the global Assimp scene object
	const aiScene* gScene = nullptr;
	// images / texture
	std::unordered_map<std::string, std::tuple<std::reference_wrapper<GLuint>, GLuint64>> textureHandleMap;	// map image filenames to textureIds and resident handles
	std::vector<GLuint> textureIds;
	GLuint invalidHandle = -1u;
	// Create an instance of the Importer class
	Assimp::Importer importer;

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

		glUseProgram(program);
		bindShaderInputs();
		GLint blockSize;
		glGetActiveUniformBlockiv(program, shaderInputs.uCalibration.index, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

		auto calibrationForShader = calibration.forShader();
		if (blockSize != sizeof(calibrationForShader))
		{
			std::cerr << "Bad shader memory layout. Calibration block is " << blockSize << " bytes, but should be " << sizeof(calibrationForShader) << std::endl;
		}

		glGenBuffers(1, &uCalibrationHandle);
		glBindBuffer(GL_UNIFORM_BUFFER, uCalibrationHandle);
		glBufferData(GL_UNIFORM_BUFFER, blockSize, (const void*)&calibrationForShader, GL_STATIC_READ);
		glBindBufferBase(GL_UNIFORM_BUFFER, shaderInputs.uCalibration.location, uCalibrationHandle);//For explanation: https://stackoverflow.com/questions/54955186/difference-between-glbindbuffer-and-glbindbufferbase

		glGenBuffers(1, &bufferHandles.objects);
		glBindBuffer(GL_UNIFORM_BUFFER, bufferHandles.objects);
		glBufferData(GL_UNIFORM_BUFFER, 0, objects.data(), GL_STATIC_READ);
		glBindBufferBase(GL_UNIFORM_BUFFER, shaderInputs.uObjects.location, bufferHandles.objects);

		createFlexibleBuffer(bufferHandles.vertex, shaderInputs.Attribute.location, vertexAttrs);

		glGenBuffers(1, &bufferHandles.triangles);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.triangles);
		glBufferData(GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(FastTriangle), triangles.data(), GL_STATIC_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, shaderInputs.Triangles.location, bufferHandles.triangles);

		createFlexibleBuffer(bufferHandles.material, shaderInputs.Material.location, materials);

		glBindVertexArray(fullScreenVAO);
		glUniform1f(shaderInputs.uTime, 0);
		// These are one-time set shaderInputs
		glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
		glUniform2f(shaderInputs.uWindowPos, windowPosX, windowPosY);
		glUniform2f(shaderInputs.uMouse, 0.5f, 0.5f);

		person.Camera.SetProjectionMatrixPerspective(fov, windowWidth / windowHeight, nearPlane, farPlane);
		person.Camera.SetCenter(glm::vec2(windowWidth / 2, windowHeight / 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		loadTestScene();
		updateBuffers();
	}
	void createFlexibleBuffer(GLuint& bufferHandle, GLuint index, anyVector& buffer)
	{
		glGenBuffers(1, &bufferHandle);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandle);
		glBufferData(GL_SHADER_STORAGE_BUFFER, buffer.totalSize, buffer.buffer.view().data(), GL_STATIC_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, bufferHandle);
	}

	void updateFlexibleBuffer(GLuint& bufferHandle, anyVector& buffer)
	{
		glNamedBufferData(bufferHandle, buffer.totalSize, buffer.buffer.view().data(), GL_STATIC_READ);
	}

	template<typename T>
	void updateFlexibleBuffer(GLuint& bufferHandle, std::vector<T>& buffer)
	{
		glNamedBufferData(bufferHandle, buffer.size() * sizeof(T), buffer.data(), GL_STATIC_READ);
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
			GlobalScreenType = ScreenType::LookingGlass;
			std::cout << "Calibration success: " << std::endl << calibration;
		}
		catch (const alert_exception& e)
		{
			calibrationAlert = e.what();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			std::cerr << "Calibration failed. Displaying flat screen version. LG version will use default values." << std::endl;
		}
	}

	void bindShaderInputs()
	{
		shaderInputs = {
			glGetUniformLocation(program, "uTime"),
			glGetUniformLocation(program, "uWindowSize"),
			glGetUniformLocation(program, "uWindowPos"),
			glGetUniformLocation(program, "uMouse"),
			glGetUniformLocation(program, "uView"),
			glGetUniformLocation(program, "uProj"),
			glGetUniformLocation(program, "uViewCone"),
			glGetUniformLocation(program, "uFocusDistance"),
			glGetUniformLocation(program, "uObjectCount"),
			{
				glGetUniformBlockIndex(program, "CalibrationBuffer")
			},
			{
				glGetUniformBlockIndex(program, "ObjectBuffer")
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,  "AttributeBuffer"),
				2
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,   "TriangleBuffer"),
				3
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,   "MaterialBuffer"),
				4
			},
		};
		glGetActiveUniformBlockiv(program, shaderInputs.uCalibration.index, GL_UNIFORM_BLOCK_BINDING, &shaderInputs.uCalibration.location);
		glGetActiveUniformBlockiv(program, shaderInputs.uObjects.index, GL_UNIFORM_BLOCK_BINDING, &shaderInputs.uObjects.location);
		/*glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, shaderInputs.Vertex.index, &shaderInputs.Vertex.location);
		glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, shaderInputs.Index.index, &shaderInputs.Index.location);
		glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, shaderInputs.Material.index, &shaderInputs.Material.location);*/
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

	const char* textureLoadingFailed = "Failed to load a texture";
	const char* sceneLoadingFailed = "Failed to load scene";
	void render() override
	{
		ui();
		glBindVertexArray(fullScreenVAO);
		glUseProgram(program);
		glUniform1f(shaderInputs.uTime, frame);
		glUniformMatrix4fv(shaderInputs.uView, 1, false, glm::value_ptr(person.Camera.GetViewMatrix()));
		glUniformMatrix4fv(shaderInputs.uProj, 1, false, glm::value_ptr(person.Camera.GetProjectionMatrix()));
		glUniform1f(shaderInputs.uViewCone, glm::radians(viewCone));
		glUniform1f(shaderInputs.uFocusDistance, focusDistance);
		glUniform2f(shaderInputs.uMouse, mouseX, mouseY);

		// Draw full screen quad with the path tracer shader
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		frame++;
	}

	void submitObjectBuffer()
	{
		updateFlexibleBuffer(bufferHandles.objects, objects);
		glUniform1ui(shaderInputs.uObjectCount, objects.size());
	}

	void clearTextures()
	{
		if (textureIds.size())
		{
			glDeleteTextures(textureIds.size(), textureIds.data());
			textureIds.clear();
			textureHandleMap.clear();
		}
	}

	glm::vec3 getRingPoint(float R, float N, float i)
	{
		auto alpha = (2.0f * glm::pi<glm::float32>() / N) * i;
		return R * glm::vec3(glm::sin(alpha), 0, glm::cos(alpha));
	}

	glm::vec3 getSubRingPoint(float M, float j, glm::vec3 center)
	{
		auto beta = (2.0f * glm::pi<glm::float32>() / M) * j;
		return center + (glm::cos(beta) * glm::normalize(center)
			+ glm::vec3(0, 1, 0) * glm::sin(beta));
	}

	//https://gist.github.com/ciembor/1494530
	/*
	 * Converts an HUE to r, g or b.
	 * returns float in the set [0, 1].
	 */
	float hue2rgb(float p, float q, float t) {

		if (t < 0)
			t += 1;
		if (t > 1)
			t -= 1;
		if (t < 1. / 6)
			return p + (q - p) * 6 * t;
		if (t < 1. / 2)
			return q;
		if (t < 2. / 3)
			return p + (q - p) * (2. / 3 - t) * 6;

		return p;

	}

	////////////////////////////////////////////////////////////////////////

	/*
	 * Converts an HSL color value to RGB. Conversion formula
	 * adapted from http://en.wikipedia.org/wiki/HSL_color_space.
	 * Assumes h, s, and l are contained in the set [0, 1] and
	 * returns RGB in the set [0, 255].
	 */
	glm::vec3 hsl2rgb(float h, float s, float l) {

		glm::vec3 result;

		if (0 == s) {
			result.r = result.g = result.b = l; // achromatic
		}
		else {
			float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
			float p = 2 * l - q;
			result.r = hue2rgb(p, q, h + 1. / 3);
			result.g = hue2rgb(p, q, h);
			result.b = hue2rgb(p, q, h - 1. / 3);
		}

		return result;

	}

	bool loadTestScene()
	{
		triangles.push_back(toFast(
			glm::vec3(0.5, 1, 1),
			glm::vec3{ 1, 0, 1 },//pos
			glm::vec3{ 0, 0, 1 },//pos,
			glm::uvec3(0, 1, 2)
		));
		vertexAttrs.push({
			glm::vec3{ 1,0,0 },//color
			glm::vec3{ 0,1,0 },//color
			glm::vec3{ 0,0,1 }//color
			}
		);


		objects.push_back(SceneObject{
			0,//material
				0,//VBO position in global vertex buffer
				0,//first index index
				1,//total triangles
				glm::uvec4(3,0,0,0)//attribute sizes (does not include vertex pos, because it is implicit)
			});


		materials.push(Material<color, color, color, color, color>{
			{1.f, 1.f, 1.f},
			{ 1.f,1.f,1.f },
			{ 1.f,1.f,1.f },
			{ 1.f,1.f,1.f },
			{ 1.f,1.f,1.f }
		});

		uint32_t vboCursor = vertexAttrs.totalSize / sizeof(float);
		uint32_t triCursor = triangles.size();

		const float R = 5.0f;
		const float N = 10;
		const float M = 10;
		int index = 0;
		std::vector<uint32_t> indices;
		std::vector<glm::vec3> vertices;
		for (float i = 0; i < N; i++)
		{
			auto pointOnRing = getRingPoint(R, N, i);
			GLuint firstIndexInSubring = index;
			for (float j = 0; j < M - 1; j += 2)
			{
				auto onSubRing = getSubRingPoint(M, j, pointOnRing);
				auto nextRing = getRingPoint(R, N, i + 1);
				auto onNextSubRing = getSubRingPoint(M, j + 2, nextRing);
				auto onPrevSubRing = getSubRingPoint(M, j, nextRing);
				if ((int)j % 2 == 0)
				{
					vertexAttrs.push(
						hsl2rgb(i / N, 1, j / M)
					);
					vertices.push_back(onSubRing);
					indices.push_back(index++);
				}
				else
				{
					indices.push_back(firstIndexInSubring + 1);
				}
				if (j == 0)
				{
					vertexAttrs.push(
						hsl2rgb((i + 1) / N, 1, j / M)
					);
					vertices.push_back(onPrevSubRing);
					indices.push_back(index++);
				}
				else
				{
					indices.push_back(index - 2);
				}
				if ((int)j % 2 == 0)
				{
					vertexAttrs.push(
						hsl2rgb((i + 1) / N, 1, (j + 2) / M)
					);
					vertices.push_back(onNextSubRing);

					indices.push_back(index++);
				}
				else
				{
					indices.push_back(firstIndexInSubring + 2);
				}

				//
				// Second triangle in a quad
				//
				indices.push_back(index - 1);
				if (j == 0)
				{
					indices.push_back(index - 3);
				}
				else
				{
					indices.push_back(index - 2);
				}


				if (j == M - 2)
				{
					// The last vertex is the same as the first
					indices.push_back(firstIndexInSubring);
				}
				else
				{
					indices.push_back(index);
				}
			}
		}
		for (std::size_t i = 0; i < indices.size(); i += 3)
		{
			triangles.push_back(toFast(
				vertices[indices[i]],
				vertices[indices[i + 1]],
				vertices[indices[i + 2]],
				glm::uvec3(indices[i], indices[i + 1], indices[i + 2])
			));
		}


		objects.push_back(SceneObject
			{//attribute sizes
				0,//material
				vboCursor,//VBO position in global vertex buffer
				triCursor,//first index index
				(uint32_t)indices.size() / 3,//total indices
				glm::uvec4(3,0,0,0)//attribute sizes
			}
		);

		//std::cout << vertices.debug(true).rdbuf();
		return true;
	}

	float f;
	int counter = 0;
	float frame = 1;
	bool interactive = false;
	bool focal = false;
	bool fullscreen = false;

	void applyScreenType()
	{
		if (GlobalScreenType == ScreenType::LookingGlass)
		{
			swapShaders(fFlatShader, fShader);
		}
		else
		{
			swapShaders(fShader, fFlatShader);
		}
	}

	void renderOnEvent(std::deque<SDL_Event>e) override
	{
		AppWindow::renderOnEvent(e);
		render();
	}

	void ui()
	{
		const char* calibrationAlertText = "Calibration Warning";
		if (!calibrationAlert.empty())
		{
			ImGui::OpenPopup(calibrationAlertText);
		}
		if (ProjectSettings::applyScreenType)
		{
			ProjectSettings::applyScreenType = false;
			applyScreenType();
		}
		if (ProjectSettings::recompileFShaders)
		{
			recompileFShaders = false;
			recompileFragmentSh();
			GlHelpers::linkProgram(program);
			bindShaderInputs();
			glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
			updateBuffers();
		}
		if (ProjectSettings::debugOutput)
		{
			glEnable(GL_DEBUG_OUTPUT);
		}
		else
		{
			glDisable(GL_DEBUG_OUTPUT);
		}
		if (ProjectSettings::reloadScene)
		{
			ProjectSettings::reloadScene = false;
			objects.clear();
			vertexAttrs.clear();
			triangles.clear();
			materials.clear();
			clearTextures();

			if (Import3DFromFile(ProjectSettings::scene.path))
			{
				if (true || LoadGLTextures(gScene))
				{
					SubmitScene(gScene, nullptr, aiMatrix4x4(scene.scale, aiQuaternion(scene.rotationDeg.x, scene.rotationDeg.y, scene.rotationDeg.z), scene.position));
				}
				else
				{
					ImGui::OpenPopup(textureLoadingFailed);
					std::cerr << "Texture loading failed \n";
				}
			}
			else
			{
				ImGui::OpenPopup(sceneLoadingFailed);
				std::cerr << "Scene loading failed \n";
			}

			updateBuffers();
			std::cout << "Scene " << scene.path.filename() << " loaded." << std::endl
				<< "Total:\n"
				<< "Obj " << objects.size() << " (" << objects.size() * sizeof(SceneObject) << " bytes)" << std::endl
				<< "Attr " << vertexAttrs.types.size() << " (" << vertexAttrs.totalSize << " bytes)" << std::endl
				<< "Tri " << triangles.size() << " (" << triangles.size() * sizeof(FastTriangle) << " bytes)" << std::endl
				<< "Mat " << materials.types.size() << " (" << materials.totalSize << " bytes)" << std::endl
				<< "Tex " << textureHandleMap.size() << std::endl;
		}
		if (ImGui::BeginPopup(textureLoadingFailed))
		{
			ImGui::TextWrapped("Encountered error when loading texture");
			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup(sceneLoadingFailed))
		{
			ImGui::TextWrapped("Encountered error when loading scene");
			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::Begin("Info");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
		if (ImGui::BeginPopup(calibrationAlertText))
		{
			ImGui::TextWrapped(calibrationAlert.c_str());
			if (ImGui::Button("Close"))
			{
				calibrationAlert.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void updateBuffers()
	{
		updateFlexibleBuffer(bufferHandles.vertex, vertexAttrs);
		updateFlexibleBuffer(bufferHandles.triangles, triangles);
		updateFlexibleBuffer(bufferHandles.material, materials);
		submitObjectBuffer();
	}

	bool workOnEvent(SDL_Event event, float deltaTime) override
	{
		if (interactive)
		{
			person.Update(deltaTime, true, event);
		}
		switch (event.type)
		{
		case SDL_KEYUP:
			switch (event.key.keysym.sym)
			{
			case SDL_KeyCode::SDLK_i:
				interactive = !interactive;
				SDL_CaptureMouse(SDL_bool(interactive));
				SDL_SetRelativeMouseMode(SDL_bool(interactive));
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
				if (GlobalScreenType == ScreenType::Flat)
				{
					GlobalScreenType = ScreenType::LookingGlass;
				}
				else
				{
					GlobalScreenType = ScreenType::Flat;
				}
				ProjectSettings::applyScreenType = true;
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
		bindShaderInputs();
		glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
	}

	void resized() override
	{
		AppWindow::resized();
		glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
		person.Camera.SetProjectionMatrixPerspective(fov, windowWidth / windowHeight, nearPlane, farPlane);
	}
	void moved() override
	{
		AppWindow::moved();
		glUniform2f(shaderInputs.uWindowPos, windowPosX, windowPosY);
	}

	//
	// Scene loading
	//
	void createAILogger()
	{
		// Change this line to normal if you not want to analyse the import process
		//Assimp::Logger::LogSeverity severity = Assimp::Logger::NORMAL;
		Assimp::Logger::LogSeverity severity = Assimp::Logger::VERBOSE;

		// Create a logger instance for Console Output
		Assimp::DefaultLogger::create("", severity, aiDefaultLogStream_STDOUT);

		// Create a logger instance for File Output (found in project folder or near .exe)
		Assimp::DefaultLogger::create("scene-loader.txt", severity, aiDefaultLogStream_FILE);

		// Now I am ready for logging my stuff
		Assimp::DefaultLogger::get()->info("this is my info-call");
	}

	void destroyAILogger()
	{
		// Kill it after the work is done
		Assimp::DefaultLogger::kill();
	}

	template<typename... Args>
	void logInfo(const Args&... arguments)
	{
		// Will add message to File with "info" Tag
		Assimp::DefaultLogger::get()->info(arguments...);
	}

	void logDebug(const char* logString)
	{
		// Will add message to File with "debug" Tag
		Assimp::DefaultLogger::get()->debug(logString);
	}


	bool Import3DFromFile(const std::filesystem::path& pFile)
	{
		// Check if file exists
		std::ifstream fin(pFile);
		if (!fin.fail())
		{
			fin.close();
		}
		else
		{
			std::cerr << "Could not open scene file " << pFile << std::endl;
			logInfo(importer.GetErrorString());
			return false;
		}

		gScene = importer.ReadFile(pFile.string(), aiProcessPreset_TargetRealtime_Quality);

		// If the import failed, report it
		if (!gScene)
		{
			logInfo(importer.GetErrorString());
			return false;
		}

		// Now we can access the file's contents.
		logInfo("Import of scene ", pFile, " succeeded.");

		// We're done. Everything will be cleaned up by the importer destructor
		return true;
	}

	int LoadGLTextures(const aiScene* scene)
	{
		//freeTextureIds();

		//ILboolean success;

		/* Before calling ilInit() version should be checked. */
		/*if (ilGetInteger(IL_VERSION_NUM) < IL_VERSION)
		{
			/// wrong DevIL version ///
			std::string err_msg = "Wrong DevIL version. Old devil.dll in system32/SysWow64?";
			char* cErr_msg = (char *) err_msg.c_str();
			abortGLInit(cErr_msg);
			return -1;
		}*/

		//ilInit(); /* Initialization of DevIL */

		if (scene->HasTextures()) return 1;
		//abortGLInit("Support for meshes with embedded textures is not implemented");

	/* getTexture Filenames and Numb of Textures */
		for (unsigned int m = 0; m < scene->mNumMaterials; m++)
		{
			int texIndex = 0;
			aiReturn texFound = AI_SUCCESS;

			aiString path;	// filename

			while (texFound == AI_SUCCESS)
			{
				texFound = scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, texIndex, &path);
				textureHandleMap.emplace(path.data, std::make_tuple(
					std::reference_wrapper(invalidHandle), GLuint64(-1)
				)); //fill map with texture paths, handles are still pseudo-NULL yet
				texIndex++;
			}
		}

		const size_t numTextures = textureHandleMap.size();


		/* array with DevIL image IDs */
		//ILuint* imageIds = NULL;
	//	imageIds = new ILuint[numTextures];

		/* generate DevIL Image IDs */
	//	ilGenImages(numTextures, imageIds); /* Generation of numTextures image names */

		///
		/// Create and fill array with GL texture names
		/// 
		textureIds.resize(numTextures);
		glCreateTextures(GL_TEXTURE_2D, static_cast<GLsizei>(numTextures), textureIds.data()); /* Texture name generation */

		/* get iterator */
		decltype(textureHandleMap)::iterator itr = textureHandleMap.begin();

		std::filesystem::path sceneDir = std::filesystem::absolute(ProjectSettings::scene.path).parent_path();
		for (size_t i = 0; i < numTextures; i++, itr++)
		{
			//save IL image ID
			std::string filename = (*itr).first;		// get filename
			std::get<0>((*itr).second) = textureIds[i]; // save texture id for filename in map

			//ilBindImage(imageIds[i]); /* Binding of DevIL image name */
			std::filesystem::path fileloc = sceneDir / filename;	/* Loading of image */
			//success = ilLoadImage(fileloc.c_str());
			int w, h, channelsCount;
			GLenum format;
			GLint internalFormat;
			unsigned char* data = stbi_load(fileloc.string().c_str(), &w, &h, &channelsCount, STBI_rgb_alpha);

			if (channelsCount == 4) {
				format = GL_RGBA;
				internalFormat = GL_COMPRESSED_RGBA;
			}
			if (channelsCount == 3) {
				format = GL_RGB;
				internalFormat = GL_COMPRESSED_RGB;
			}
			if (channelsCount == 2) {
				format = GL_RG;
				internalFormat = GL_COMPRESSED_RG;
			}
			if (channelsCount == 1) {
				format = GL_RED;
				internalFormat = GL_COMPRESSED_RED;
			}

			if (nullptr != data)
			{
				// Convert every colour component into unsigned byte.If your image contains
				// alpha channel you can replace IL_RGB with IL_RGBA
				//success = ilConvertImage(IL_RGB, IL_UNSIGNED_BYTE);
				/*if (!success)
				{
					abortGLInit("Couldn't convert image");
					return -1;
				}*/
				// Binding of texture name
				auto& currentTex = textureIds[i];
				// redefine standard texture values
				// We will use linear interpolation for magnification filter
				glTextureImage2DEXT(currentTex, GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, data);
				glTextureParameteri(currentTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				// We will use linear interpolation for minifying filter
				glTextureParameteri(currentTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				// we also want to be able to deal with odd texture dimensions
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
				glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

				glGenerateMipmap(GL_TEXTURE_2D);
				auto bindlessHandle = glGetTextureHandleARB(currentTex);
				glMakeTextureHandleResidentARB(bindlessHandle);
				std::get<1>((*itr).second) = bindlessHandle;
				stbi_image_free(data);
			}
			else
			{
				/* Error occurred */
				std::cerr << "Couldn't load Image: " << fileloc << std::endl;
			}
		}
		// Because we have already copied image data into texture data  we can release memory used by image.
	//	ilDeleteImages(numTextures, imageIds);

		// Cleanup
		//delete [] imageIds;
		//imageIds = NULL;

		return TRUE;
	}

	void SubmitMaterial(const aiMaterial* mtl)
	{
		int ret1, ret2;
		aiColor4D diffuse;
		aiColor4D specular;
		aiColor4D ambient;
		aiColor4D emission;
		ai_real shininess, strength;
		int two_sided;
		int wireframe;
		unsigned int max;

		int texIndex = 0;
		aiString texPath;	//contains filename of texture

		if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, texIndex, &texPath))
		{
			//bind texture
			auto& handles = textureHandleMap.at(texPath.data);
			auto& texId = std::get<0>(handles);
			auto& texHandle = std::get<1>(handles);
			// create a handle for bindless texture shader
			//14650633544276105767.jpg
			materials.push(Material<textureHandle, color, color, color, color>(
				texHandle, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }
				)
			);
		}
		//set_float4(c, 0.8f, 0.8f, 0.8f, 1.0f);
		else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
		{
			//color4_to_float4(&diffuse, c);
			materials.push(Material<color, color, color, color, color>(
				{ diffuse.r ,diffuse.g, diffuse.b }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }, { 0,0,0 }
				)
			);
		}
		//glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, c);

		/*set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
			color4_to_float4(&specular, c);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);

		set_float4(c, 0.2f, 0.2f, 0.2f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
			color4_to_float4(&ambient, c);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, c);

		set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
			color4_to_float4(&emission, c);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, c);

		max = 1;
		ret1 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max);
		max = 1;
		ret2 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength, &max);
		if ((ret1 == AI_SUCCESS) && (ret2 == AI_SUCCESS))
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess * strength);
		else {
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
			set_float4(c, 0.0f, 0.0f, 0.0f, 0.0f);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
		}

		max = 1;
		if (AI_SUCCESS == aiGetMaterialIntegerArray(mtl, AI_MATKEY_ENABLE_WIREFRAME, &wireframe, &max))
			fill_mode = wireframe ? GL_LINE : GL_FILL;
		else
			fill_mode = GL_FILL;
		glPolygonMode(GL_FRONT_AND_BACK, fill_mode);

		max = 1;
		if ((AI_SUCCESS == aiGetMaterialIntegerArray(mtl, AI_MATKEY_TWOSIDED, &two_sided, &max)) && two_sided)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
			*/
	}

	std::size_t pushAttributes(const aiMesh* mesh, std::size_t& index, const aiMatrix4x4& transMat)
	{
		//auto uvNum = mesh->GetNumUVChannels();
		auto uvNum = mesh->HasTextureCoords(0) ? 1 : 0;

		std::size_t attrFloatsCount = 0;
		if (mesh->mNormals != nullptr)// If the mesh has vertex colors
		{
			auto norm = mesh->mNormals[index];
			vertexAttrs.push(norm);
			attrFloatsCount += 3;
		}
		for (unsigned int c = 0; c < mesh->GetNumColorChannels(); c++)
		{
			auto col = mesh->mColors[c][index];
			vertexAttrs.push(col);
			attrFloatsCount += 4;
		}
		for (unsigned int t = 0; t < uvNum; t++)
		{
			assert(mesh->mNumUVComponents[t] == 2);
			//for (auto t = (decltype(mesh->mNumUVComponents[0]))0;
			//	t < mesh->mNumUVComponents[0]; t++)
			{
				auto tCor = mesh->mTextureCoords[t][index];
				vertexAttrs.push(tCor.x);
				vertexAttrs.push(tCor.y);
				attrFloatsCount += 2;
			}
		}
		return attrFloatsCount;
	}

	void SubmitScene(const struct aiScene* sc, const struct aiNode* nd = nullptr, aiMatrix4x4 parentTransform = aiMatrix4x4())
	{
		if (nd == nullptr)
		{
			nd = sc->mRootNode;
		}
		aiMatrix4x4 transMat = parentTransform * nd->mTransformation;

		/*aiMatrix4x4 m2;
		aiMatrix4x4::Scaling(aiVector3D(scale, scale, scale), m2);
		m = m * m2;

		// update transform
		m.Transpose();
		glPushMatrix();
		glMultMatrixf((float*)&m);*/

		// draw all meshes assigned to this node
		for (auto n = (decltype(nd->mNumMeshes))0; n < nd->mNumMeshes; ++n)
		{
			if (objects.size() >= objectCountLimit)
			{
				return;
			}
			const struct aiMesh* mesh = sc->mMeshes[nd->mMeshes[n]];

			auto vboCursorPos = vertexAttrs.totalSize / sizeof(float);
			auto triCursorPos = triangles.size();
			auto materialIndex = materials.totalSize / sizeof(float);

			for (std::size_t v = 0; v < mesh->mNumVertices; v++)
			{
				pushAttributes(mesh, v, transMat);
			}
			// Construct triangle lookup table
			for (std::size_t f = 0; f < mesh->mNumFaces; f++)
			{
				const struct aiFace* face = &mesh->mFaces[f];
				assert(face->mNumIndices == 3);// Support triangles only

				// Apply transformation matrix and construct a trinagle
				glm::uvec3 indices = { face->mIndices[0], face->mIndices[1], face->mIndices[2] };
				glm::vec3 v0 = GlHelpers::aiToGlm(transMat * mesh->mVertices[indices.x]);
				glm::vec3 v1 = GlHelpers::aiToGlm(transMat * mesh->mVertices[indices.y]);
				glm::vec3 v2 = GlHelpers::aiToGlm(transMat * mesh->mVertices[indices.z]);
				auto fastTri = toFast(v0, v1, v2, indices);
				triangles.push_back(fastTri);
			}

			if (mesh->mNormals != nullptr)
			{
				objects.push_back(SceneObject(
					materialIndex, vboCursorPos, triCursorPos, mesh->mNumFaces,
					glm::uvec4(3, 0, 0, 0)
				)
				);
			}
			else
			{
				objects.push_back(SceneObject(
					materialIndex, vboCursorPos, triCursorPos, mesh->mNumFaces,
					glm::uvec4(2, 0, 0, 0)
				)
				);
			}

			//SubmitMaterial(sc->mMaterials[mesh->mMaterialIndex]);
		}

		// draw all children
		for (auto n = (decltype(nd->mNumChildren))0; n < nd->mNumChildren; ++n)
		{
			SubmitScene(sc, nd->mChildren[n], parentTransform);
		}
	}
};