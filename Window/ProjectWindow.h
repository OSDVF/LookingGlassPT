#pragma once
#include <SDL.h>
#include <fstream>
#include <sstream>
#include <array>
#include <string>
#include <limits>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../GlHelpers.h"
#include "../Helpers.h"
#include "../Structures/SceneAndViewSettings.h"
#include "../Structures/SceneObjects.h"
#include "../Structures/Bvh.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace SceneAndViewSettings;
class ProjectWindow : public AppWindow {
public:
	GLuint fShader;
	GLuint fFlatShader;
	GLuint vShader;
	GLuint program;
	GLuint fullScreenVAO;
	GLuint fullScreenVertexBuffer;
	GLuint uCalibrationHandle;
	struct {
		GLuint vertex;
		GLuint triangles;
		GLuint objects;
		GLuint material;
		GLuint lights;
		GLuint bvh;
	} bufferHandles;
	struct BufferDefinition {
		GLuint index;
		GLint location;
	};
	struct ImageDefinition {
		GLint location;
		GLuint unit;
		GLuint texture;
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
		GLint uInvRayCount;
		GLint uRayIndex;
		GLint uRayOffset;
		GLint uSubpI;
		BufferDefinition uCalibration;
		BufferDefinition uObjects;
		ImageDefinition uScreenAlbedo;
		ImageDefinition uScreenNormal;
		ImageDefinition uScreenColorDepth;
		BufferDefinition Attribute;
		BufferDefinition Triangles;
		BufferDefinition Material;
		BufferDefinition Lights;
		BufferDefinition BVH;
	} shaderInputs;

	anyVector vertexAttrs;
	// The rendering is non-indexed
	std::vector<FastTriangleFirstHalf> trianglesFirst;
	std::vector<FastTriangleSecondHalf> trianglesSecond;
	std::vector<SceneObject> objects;
	std::vector<Material> materials;
	std::vector<Light> lights;
	BVHBuilder bvhBuilder;
	uint32_t rayNumber;
	uint32_t raySalt;

	//
	// Asset Import Library Objects
	//
	const aiScene* gScene = nullptr;
	Assimp::Importer importer;
	// images / texture
	std::unordered_map<std::string, std::tuple<std::reference_wrapper<GLuint>, GLuint64>> textureHandleMap;	// map image filenames to textureIds and resident handles
	std::vector<GLuint> textureIds;
	GLuint invalidHandle = -1u;
	std::unordered_map<int, uint32_t> sceneMaterialIndices;

	std::mt19937 randomGenerator;

	ProjectWindow(const char* name, float x, float y, float w, float h)
		: AppWindow(name, x, y, w, h)
	{
		eventDriven = false;
	}

	// Runs on the render thread
	void setupGL() override
	{
		AppWindow::setupGL();
		if (SceneAndViewSettings::debugOutput != DEBUG_SEVERITY_NOTHING)
		{
			GlHelpers::initCallback();
		}

		program = glCreateProgram();
		try {
			GlHelpers::compileShader<GL_VERTEX_SHADER>(Helpers::relativeToExecutable("vertex.vert").string(), vShader, {});
		}
		catch (const std::runtime_error& e)
		{
			resourceError += e.what();
		}
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

		glGenBuffers(1, &uCalibrationHandle);
		glBindBuffer(GL_UNIFORM_BUFFER, uCalibrationHandle);
		glBindBufferBase(GL_UNIFORM_BUFFER, shaderInputs.uCalibration.location, uCalibrationHandle);//For explanation: https://stackoverflow.com/questions/54955186/difference-between-glbindbuffer-and-glbindbufferbase

		glGenBuffers(1, &bufferHandles.objects);
		glBindBuffer(GL_UNIFORM_BUFFER, bufferHandles.objects);
		glBufferData(GL_UNIFORM_BUFFER, 0, objects.data(), GL_STATIC_READ);
		glBindBufferBase(GL_UNIFORM_BUFFER, shaderInputs.uObjects.location, bufferHandles.objects);

		createFlexibleBuffer(bufferHandles.vertex, shaderInputs.Attribute.location, vertexAttrs);

		glGenBuffers(1, &bufferHandles.triangles);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.triangles);
		glBufferData(GL_SHADER_STORAGE_BUFFER, trianglesSecond.size() * sizeof(FastTriangleSecondHalf), trianglesSecond.data(), GL_STATIC_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, shaderInputs.Triangles.location, bufferHandles.triangles);

		glGenBuffers(1, &bufferHandles.material);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.material);
		glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(Material), materials.data(), GL_STATIC_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, shaderInputs.Material.location, bufferHandles.material);

		glGenBuffers(1, &bufferHandles.lights);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.lights);
		glBufferData(GL_SHADER_STORAGE_BUFFER, lights.size() * sizeof(Light), lights.data(), GL_STATIC_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, shaderInputs.Lights.location, bufferHandles.lights);

		glGenBuffers(1, &bufferHandles.bvh);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferHandles.bvh);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, shaderInputs.BVH.location, bufferHandles.bvh);

		createFullScreenImageBuffer(shaderInputs.uScreenAlbedo.texture, shaderInputs.uScreenAlbedo.unit);
		createFullScreenImageBuffer(shaderInputs.uScreenNormal.texture, shaderInputs.uScreenNormal.unit);
		createFullScreenImageBuffer(shaderInputs.uScreenColorDepth.texture, shaderInputs.uScreenColorDepth.unit, GL_RGBA16F);

		glBindVertexArray(fullScreenVAO);
		glUniform1f(shaderInputs.uTime, 0);
		// These are one-time set shaderInputs
		glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
		glUniform2f(shaderInputs.uWindowPos, windowPosX, windowPosY);
		glUniform2f(shaderInputs.uMouse, 0.5f, 0.5f);

		person.Camera.SetProjectionMatrixPerspective(fov, windowWidth / windowHeight, nearPlane, farPlane);
		person.Camera.SetCenter(glm::vec2(windowWidth / 2, windowHeight / 2));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		updateBuffers();
	}

	void createFullScreenImageBuffer(GLuint& textureId, GLuint binding, GLenum format = GL_RGBA8)
	{
		//Create the texture
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexStorage2D(GL_TEXTURE_2D, 1, format, windowWidth, windowHeight);
		glBindTexture(GL_TEXTURE_2D, 0); //Unbind the texture

		bindImage(textureId, binding, format);
	}
	void bindImage(GLuint& textureId, GLuint binding, GLenum format = GL_RGBA8)
	{
		//Use the texture as an image
		glBindImageTexture(binding, textureId, 0, GL_FALSE, 0, GL_READ_WRITE, format);
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
			glGetUniformLocation(program, "uInvRayCount"),
			glGetUniformLocation(program, "uRayIndex"),
			glGetUniformLocation(program, "uRayOffset"),
			glGetUniformLocation(program, "uSubpI"),
			{
				glGetUniformBlockIndex(program, "CalibrationBuffer")
			},
			{
				glGetUniformBlockIndex(program, "ObjectBuffer")
			},
			{
				glGetUniformLocation(program, "uScreenAlbedo"),
				2
			},
			{
				glGetUniformLocation(program, "uScreenNormal"),
				3
			},
			{
				glGetUniformLocation(program, "uScreenColorDepth"),
				4
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,  "AttributeBuffer"),
				5
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,   "TriangleBuffer"),
				6
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,   "MaterialBuffer"),
				7
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,   "LightBuffer"),
				8
			},
			{
				glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK,   "BVHBuffer"),
				9
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
		try {
			std::string fragSource = Helpers::relativeToExecutable("fragment.frag").string();
			auto bouncesDefine = fmt::format("MAX_BOUNCES {:d}", maxBounces);
			auto subpixelOnePassDefine = std::string(subpixelOnePass ? "SUBPIXEL_ONE_PASS" : "SUBPIXEL_MULTI_PASS");
			auto cullingDefine = std::string(backfaceCulling ? "CULLING" : "NO_CULLING");
			auto debugVisualizeBVHDefine = std::string(SceneAndViewSettings::visualizeBVH ? "DEBUG_VISUALIZE_BVH" : "NO_DEBUG_VISUALIZE_BVH");
			auto debugLevelMaskDefine = fmt::format("DEBUG_BVH_LEVEL_MASK 0x{:X}u", SceneAndViewSettings::bvhDebugIterationsMask);
			GlHelpers::compileShader<GL_FRAGMENT_SHADER>(fragSource, fShader, { bouncesDefine, subpixelOnePassDefine, cullingDefine, debugVisualizeBVHDefine, debugLevelMaskDefine });
			GlHelpers::compileShader<GL_FRAGMENT_SHADER>(fragSource, fFlatShader, { "FLAT_SCREEN", bouncesDefine, subpixelOnePassDefine, cullingDefine, debugVisualizeBVHDefine, debugLevelMaskDefine });
			glAttachShader(program, GlobalScreenType == ScreenType::Flat ? fFlatShader : fShader);
		}
		catch (const std::runtime_error& e)
		{
			resourceError += e.what();
			ImGui::OpenPopup(resourceLoadingFailed);
		}
	}

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
		if (!SceneAndViewSettings::subpixelOnePass && GlobalScreenType == ScreenType::LookingGlass)
		{
			switch (currentSubpixel)
			{
			case 0:
				glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
				break;
			case 1:
				glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
				break;
			case 2:
				glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
				break;
			}
			glUniform1ui(shaderInputs.uSubpI, currentSubpixel);
			currentSubpixel++;
			if (currentSubpixel >= 3)
			{
				currentSubpixel = 0;
			}
		}
		if (SceneAndViewSettings::pathTracing)
		{
			glUniform1f(shaderInputs.uInvRayCount, 1.f / ((float)rayIteration));
			glUniform1ui(shaderInputs.uRayIndex, SceneAndViewSettings::rayIteration += currentSubpixel / 2);
			glUniform1f(shaderInputs.uRayOffset, rayOffset);
			if (rayIteration > maxIterations)
			{
				SceneAndViewSettings::stopPathTracing();
			}
		}
		else
		{
			glUniform1f(shaderInputs.uInvRayCount, 1.f);
			glUniform1ui(shaderInputs.uRayIndex, 0u);
		}

		// Draw full screen quad with the path tracer shader
		if (SceneAndViewSettings::pathTracing || rayIteration == 0)
		{
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
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

	float frame = 1;
	bool focal = false;
	bool fullscreen = false;
	std::string textureErrors;
	std::string resourceError;
	const char* textureLoadingFailed = "Failed to load a texture";
	const char* resourceLoadingFailed = "Failed to load resource";
	std::size_t currentSubpixel = 2;

	void applyScreenType()
	{
		if (GlobalScreenType == ScreenType::LookingGlass)
		{
			swapShaders(fFlatShader, fShader);
		}
		else
		{
			swapShaders(fShader, fFlatShader);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
		}
	}

	void renderOnEvent(std::deque<SDL_Event>e) override
	{
		AppWindow::renderOnEvent(e);
		render();
	}
	void recreateBufferImages()
	{
		glDeleteTextures(1, &shaderInputs.uScreenAlbedo.texture);
		glDeleteTextures(1, &shaderInputs.uScreenNormal.texture);
		glDeleteTextures(1, &shaderInputs.uScreenColorDepth.texture);
		createFullScreenImageBuffer(shaderInputs.uScreenAlbedo.texture, shaderInputs.uScreenAlbedo.unit);
		createFullScreenImageBuffer(shaderInputs.uScreenNormal.texture, shaderInputs.uScreenNormal.unit);
		createFullScreenImageBuffer(shaderInputs.uScreenColorDepth.texture, shaderInputs.uScreenColorDepth.unit, GL_RGBA16F);
	}

	void ui()
	{
		if (SceneAndViewSettings::applyScreenType)
		{
			SceneAndViewSettings::applyScreenType = false;
			applyScreenType();
		}
		if (SceneAndViewSettings::recompileFShaders)
		{
			recompileFShaders = false;
			recompileFragmentSh();
			GlHelpers::linkProgram(program);
			glUseProgram(program);
			bindShaderInputs();
			recreateBufferImages();
			glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
			if (subpixelOnePass)
			{
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
				currentSubpixel = 2; //Because rayIteration is incremented by currentSubpixel/2 when doing path tracing
			}
			updateBuffers();
		}
		if (SceneAndViewSettings::reloadScene)
		{
			SceneAndViewSettings::reloadScene = false;
			clearBuffers();


			try
			{
				Import3DFromFile(SceneAndViewSettings::scene.path);

				textureErrors = LoadGLTextures(gScene);
				SubmitScene(gScene, nullptr, aiMatrix4x4(scene.scale, aiQuaternion(
					glm::radians(scene.rotationDeg.x), glm::radians(scene.rotationDeg.y), glm::radians(scene.rotationDeg.z)
				), scene.position));

				auto before = std::chrono::system_clock::now();
				bvhBuilder.sahThreshold = SceneAndViewSettings::bvhSAHthreshold;
				bvhBuilder.build(trianglesFirst, trianglesSecond);
				auto after = std::chrono::system_clock::now();
				std::cout << "BVH Construction took " << std::chrono::duration<float, std::milli>(after - before).count() << " ms";

				if (!textureErrors.empty())
				{
					ImGui::OpenPopup(textureLoadingFailed);
					std::cerr << "Texture loading failed \n";
				}
			}
			catch (const std::runtime_error& e)
			{
				resourceError = e.what();
				ImGui::OpenPopup(resourceLoadingFailed);
				std::cerr << "Resource loading failed:\n" << resourceError << std::endl;
			}

			updateBuffers();
			std::cout << "Scene " << scene.path.filename() << " loaded." << std::endl
				<< "Total:\n"
				<< "Obj " << objects.size() << " (" << objects.size() * sizeof(SceneObject) << " bytes)" << std::endl
				<< "Attr " << vertexAttrs.types.size() << " (" << vertexAttrs.totalSize << " bytes)" << std::endl
				<< "Tri " << trianglesFirst.size() << " (" << trianglesFirst.size() * sizeof(FastTriangleSecondHalf) << " bytes)" << std::endl
				<< "BVH " << bvhBuilder.m_packedNodes.size() << " (" << bvhBuilder.m_packedNodes.size() * sizeof(BVHPackedNode) << " bytes)" << std::endl
				<< "Mat " << materials.size() << " (" << materials.size() * sizeof(Material) << " bytes)" << std::endl
				<< "Tex " << textureHandleMap.size() << std::endl;
		}
		if (ImGui::BeginPopup(textureLoadingFailed))
		{
			// Enforce minimum automatic window width
			ImGui::SetCursorPosX(windowWidth / 2);
			ImGui::SetCursorPosX(10.0f);
			ImGui::TextWrapped(textureErrors.c_str());
			if (ImGui::Button("Close"))
			{
				textureErrors.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (!resourceError.empty())
		{
			ImGui::OpenPopup(resourceLoadingFailed);
		}
		if (ImGui::BeginPopup(resourceLoadingFailed))
		{
			// Enforce minimum automatic window width
			ImGui::SetCursorPosX(windowWidth / 2);
			ImGui::SetCursorPosX(10.0f);
			ImGui::TextWrapped(resourceError.c_str());
			if (ImGui::Button("Close"))
			{
				resourceError.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (fpsWindow)
		{
			ImGui::Begin("Info");
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}
	}

	void updateCalibrationBuffer()
	{
		glBindBuffer(GL_UNIFORM_BUFFER, uCalibrationHandle);
		auto calibrationForShader = calibration.forShader();
		GLint blockSize;
		glGetActiveUniformBlockiv(program, shaderInputs.uCalibration.index, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
		if (blockSize != sizeof(calibrationForShader))
		{
			std::cerr << "Bad shader memory layout. Calibration block is " << blockSize << " bytes, but should be " << sizeof(calibrationForShader) << std::endl;
		}
		glBufferData(GL_UNIFORM_BUFFER, sizeof(calibrationForShader), (const void*)&calibrationForShader, GL_STATIC_READ);
	}

	void updateBuffers()
	{
		updateFlexibleBuffer(bufferHandles.vertex, vertexAttrs);
		//Store only the second half of values inside triangle buffer. The first half is inside BVH
		updateFlexibleBuffer(bufferHandles.triangles, trianglesSecond);
		updateFlexibleBuffer(bufferHandles.material, materials);
		updateFlexibleBuffer(bufferHandles.lights, lights);
		updateFlexibleBuffer(bufferHandles.bvh, bvhBuilder.m_packedNodes);
		updateCalibrationBuffer();
		submitObjectBuffer();
	}

	void clearBuffers()
	{
		objects.clear();
		vertexAttrs.clear();
		trianglesFirst.clear();
		trianglesSecond.clear();
		materials.clear();
		lights.clear();
		sceneMaterialIndices.clear();
		clearTextures();
	}

	bool workOnEvent(SDL_Event event, float deltaTime) override
	{
		if (SceneAndViewSettings::interactive)
		{
			person.Update(deltaTime, true, event);
		}
		switch (event.type)
		{
		case SDL_KEYUP:
			switch (event.key.keysym.sym)
			{
			case SDL_KeyCode::SDLK_i:
				SceneAndViewSettings::interactive = !SceneAndViewSettings::interactive;
				SDL_CaptureMouse(SDL_bool(interactive));
				SDL_SetRelativeMouseMode(SDL_bool(interactive));
				break;
			case SDL_KeyCode::SDLK_m:
				focal = !focal;
				break;
			case SDLK_ESCAPE:
				hide();
				break;
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
				this->eventDriven = !this->eventDriven;
				break;
			case SDL_KeyCode::SDLK_r:
				SceneAndViewSettings::recompileFShaders = true;
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
				SceneAndViewSettings::applyScreenType = true;
				break;
			}
			break;
		case SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_CLOSE:
				hide();
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				interactive = false;
				SDL_CaptureMouse(SDL_FALSE);
				SDL_SetRelativeMouseMode(SDL_FALSE);
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				if (interactive)
				{
					SDL_CaptureMouse(SDL_TRUE);
					SDL_SetRelativeMouseMode(SDL_TRUE);
				}
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
		updateBuffers();
		glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
	}

	void resized() override
	{
		AppWindow::resized();
		glUniform2f(shaderInputs.uWindowSize, windowWidth, windowHeight);
		recreateBufferImages();
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


	void Import3DFromFile(const std::filesystem::path& pFile)
	{
		// Check if file exists
		std::ifstream fin(pFile);
		if (!fin.fail())
		{
			fin.close();
		}
		else
		{
			logInfo(importer.GetErrorString());
			throw std::runtime_error(fmt::format("Could not open scene file {}: \n{}", pFile.string(), importer.GetErrorString()));
		}

		gScene = importer.ReadFile(pFile.string(), aiProcessPreset_TargetRealtime_Quality | aiPostProcessSteps::aiProcess_FlipUVs | aiPostProcessSteps::aiProcess_FixInfacingNormals | aiPostProcessSteps::aiProcess_Triangulate);

		// If the import failed, report it
		if (!gScene)
		{
			logInfo(importer.GetErrorString());
			throw std::runtime_error(fmt::format("Import failed:\n{}", importer.GetErrorString()));
		}

		// Now we can access the file's contents.
		logInfo("Import of scene ", pFile, " succeeded.");

		// We're done. Everything will be cleaned up by the importer destructor
	}

	// Returns errors list
	std::string LoadGLTextures(const aiScene* scene)
	{
		std::stringstream ss;
		if (scene->HasTextures())
		{
			return "Support for meshes with embedded textures is not implemented yet.\n";
		}

		/* getTexture Filenames and Numb of Textures */
		std::cout << scene->mNumMaterials << " materials in the scene" << std::endl;
		for (unsigned int m = 0; m < scene->mNumMaterials; m++)
		{
			aiReturn texFound = AI_SUCCESS;

			aiString path;	// filename
			auto material = scene->mMaterials[m];
#if _DEBUG
			std::cout << "Material " << m << std::endl;
			for (int i = 0; i < material->mNumProperties; i++)
			{
				auto prop = material->mProperties[i];
				std::cout << "prop " << prop->mKey.C_Str() << "(semantic " << prop->mSemantic << ") data: ";
				switch (prop->mType)
				{
				case aiPropertyTypeInfo::aiPTI_Float:
					std::cout << *(float*)prop->mData;
					break;
				case aiPropertyTypeInfo::aiPTI_Double:
					std::cout << *(double*)prop->mData;
					break;
				case aiPropertyTypeInfo::aiPTI_Integer:
					std::cout << *(int*)prop->mData;
					break;
				case aiPropertyTypeInfo::aiPTI_String:
					std::cout << std::string(prop->mData, prop->mDataLength);
					break;
				}
				std::cout << std::endl;
			}
#endif
			auto texCount = material->GetTextureCount(aiTextureType_DIFFUSE);
			std::cout << "has " << texCount << " diff textures." << std::endl;
			for (int texIndex = 0; texIndex < texCount; texIndex++)
			{
				if (material->GetTexture(aiTextureType_DIFFUSE, texIndex, &path) != aiReturn_SUCCESS)
				{
					std::cerr << "texture " << texIndex << " not loaded" << std::endl;
					continue;
				}
				textureHandleMap.emplace(path.data, std::make_tuple(
					std::reference_wrapper(invalidHandle), GLuint64(-1)
				)); //fill map with texture paths, handles are still pseudo-NULL yet
			}

			texCount = material->GetTextureCount(aiTextureType_EMISSIVE);
			std::cout << "has " << texCount << " emissive textures." << std::endl;
			for (int texIndex = 0; texIndex < texCount; texIndex++)
			{
				if (material->GetTexture(aiTextureType_EMISSIVE, texIndex, &path) != aiReturn_SUCCESS)
				{
					std::cerr << "texture " << texIndex << " not loaded" << std::endl;
					continue;
				}
				textureHandleMap.emplace(path.data, std::make_tuple(
					std::reference_wrapper(invalidHandle), GLuint64(-1)
				)); //fill map with texture paths, handles are still pseudo-NULL yet
			}
		}

		const size_t numTextures = textureHandleMap.size();

		///
		/// Create and fill array with GL texture names
		/// 
		textureIds.resize(numTextures);
		glCreateTextures(GL_TEXTURE_2D, static_cast<GLsizei>(numTextures), textureIds.data()); /* Texture name generation */

		/* get iterator */
		decltype(textureHandleMap)::iterator itr = textureHandleMap.begin();

		std::filesystem::path sceneDir = std::filesystem::absolute(SceneAndViewSettings::scene.path).parent_path();
		for (size_t i = 0; i < numTextures; i++, itr++)
		{
			std::string filename = (*itr).first;		// get filename
			std::get<0>((*itr).second) = textureIds[i]; // save texture id for filename in map

			std::filesystem::path fileloc = sceneDir / filename;	/* Loading of image */
			int w, h, channelsCount;
			GLenum format = 0;
			GLint internalFormat = 0;
			unsigned char* data = stbi_load(fileloc.string().c_str(), &w, &h, &channelsCount, STBI_rgb_alpha);

			switch (channelsCount)
			{
			case 4:
				format = GL_RGBA;
				internalFormat = GL_COMPRESSED_SRGB_ALPHA;
				break;
			case 3:
				format = GL_RGB;
				internalFormat = GL_COMPRESSED_SRGB;
				break;
			case 2:
				format = GL_RG;
				internalFormat = GL_COMPRESSED_RG;
				break;
			case 1:
				format = GL_RED;
				internalFormat = GL_COMPRESSED_RED;
				break;
			default:
				ss << "Texture " << i << " has unsupported channel count of " << channelsCount << ".\n";
				continue;
			}

			if (nullptr != data)
			{
				// Binding of texture name
				auto& currentTex = textureIds[i];
				// redefine standard texture values
				// We will use linear interpolation for magnification filter
				glTextureImage2DEXT(currentTex, GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
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
				// Error occurred
				if (ss.rdbuf()->in_avail() == 0)
				{
					//is still empty
					ss << "Encountered error(s) when loading texture(s):" << std::endl;
				}
				ss << "Couldn't load Image: " << fileloc << std::endl;
			}
		}
		std::cerr << ss.rdbuf();
		return ss.str();
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
		Material newMat;
		if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, texIndex, &texPath) && CheckTextureExistence(texPath, mtl))
		{
			auto& handleAndId = textureHandleMap.at(texPath.data);
			auto& texHandle = std::get<1>(handleAndId);
			// pass a handle to a bindless texture in the material
			newMat = Material(texHandle);
		}
		else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
		{
			//color4_to_float4(&diffuse, c);
			newMat = Material(
				glm::vec3(diffuse.r, diffuse.g, diffuse.b)
			);
			newMat.transparency = diffuse.a;
		}
		if (AI_SUCCESS == aiGetMaterialTexture(mtl, aiTextureType_EMISSIVE, texIndex, &texPath) && CheckTextureExistence(texPath, mtl))
		{
			auto& handleAndId = textureHandleMap.at(texPath.data);
			auto& texHandle = std::get<1>(handleAndId);
			newMat.setEmissive(texHandle);
		}
		else if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
		{
			newMat.setEmissive(GlHelpers::aiToGlm(emission) * lightMultiplier);
		}
		materials.push_back(newMat);
	}

	bool CheckTextureExistence(aiString& texPath, const aiMaterial* mtl)
	{
		if (!textureHandleMap.contains(texPath.data))
		{
			std::cerr << "Material " << mtl->GetName().C_Str() << " needs texture " << texPath.C_Str() << " but it wasn't loaded properly." << std::endl;
			return false;
		}
		return true;
	}

	int getUvNum(const aiMesh* mesh)
	{
		//auto uvNum = mesh->GetNumUVChannels();
		auto uvNum = mesh->HasTextureCoords(0) ? 1 : 0;
		return uvNum;
	}

	void pushAttributes(const aiMesh* mesh, std::size_t& index, const aiMatrix4x4& transMat, const aiMatrix3x3& normalTransMat)
	{
		auto uvNum = getUvNum(mesh);
		if (mesh->HasVertexColors(0))// If the mesh has vertex colors
		{
			auto col = mesh->mColors[0][index];
			vertexAttrs.push(GlHelpers::aiToGlm(col));
		}
		if (mesh->mNormals != nullptr)
		{
			auto norm = normalTransMat * mesh->mNormals[index];
			vertexAttrs.push(GlHelpers::aiToGlm(norm));
		}
		for (unsigned int t = 0; t < uvNum; t++)
		{
			//for (auto t = (decltype(mesh->mNumUVComponents[0]))0;
			//	t < mesh->mNumUVComponents[0]; t++)
			{
				auto tCor = mesh->mTextureCoords[t][index];
				vertexAttrs.push(float(tCor.x));
				vertexAttrs.push(float(tCor.y));
			}
		}
	}

	void SubmitScene(const struct aiScene* sc, const struct aiNode* nd = nullptr, aiMatrix4x4 transformationMatrix = aiMatrix4x4())
	{
		if (nd == nullptr)
		{
			nd = sc->mRootNode;
		}
		transformationMatrix = transformationMatrix * nd->mTransformation;

		// draw all meshes assigned to this node
		for (auto n = (decltype(nd->mNumMeshes))0; n < nd->mNumMeshes; ++n)
		{
			std::cout << nd->mName.C_Str() << " has a mesh\n";
			glm::vec3 aabbMax(-std::numeric_limits<float>::infinity());
			glm::vec3 aabbMin(std::numeric_limits<float>::infinity());
			glm::vec3 averageNormal = glm::vec3(0, -1, 0);

			if (objects.size() >= objectCountLimit)
			{
				return;
			}
			const struct aiMesh* mesh = sc->mMeshes[nd->mMeshes[n]];

			for (unsigned int t = 0; t < getUvNum(mesh); t++)
			{
				if (mesh->mNumUVComponents[t] != 2)
				{
					resourceError += "Only meshes with two-dimensional UVs are supported yet.";
				}
			}

			auto vboCursorPos = vertexAttrs.totalSize / sizeof(float);
			auto triCursorPos = trianglesFirst.size();
			auto materialCursorPos = materials.size();
			auto lightCursorPos = lights.size();

			std::size_t v = 0;
			auto normalTransMat = aiMatrix3x3(transformationMatrix).Inverse().Transpose();
			pushAttributes(mesh, v, transformationMatrix, normalTransMat);
			if (mesh->mNormals != nullptr)
			{
				averageNormal = GlHelpers::aiToGlm(normalTransMat * mesh->mNormals[v]);
			}
			for (v = 1; v < mesh->mNumVertices; v++)
			{
				pushAttributes(mesh, v, transformationMatrix, normalTransMat);
				float invVertNumber = 1.f / ((float)(v + 1.f));
				averageNormal = glm::mix(GlHelpers::aiToGlm(normalTransMat * mesh->mNormals[v]), averageNormal, invVertNumber);
			}
			auto objectIndex = (uint32_t)objects.size();

			// Construct triangle lookup table
			for (std::size_t f = 0; f < mesh->mNumFaces; f++)
			{
				const struct aiFace* face = &mesh->mFaces[f];
				if (face->mNumIndices != 3)// Support triangles only
				{
					resourceError += "Only triangle meshes are supported yet.";
					return;
				}

				// Apply transformation matrix and construct a trinagle
				glm::uvec3 indices = { face->mIndices[0], face->mIndices[1], face->mIndices[2] };
				glm::vec3 v0 = GlHelpers::aiToGlm(transformationMatrix * mesh->mVertices[indices.x]);
				glm::vec3 v1 = GlHelpers::aiToGlm(transformationMatrix * mesh->mVertices[indices.y]);
				glm::vec3 v2 = GlHelpers::aiToGlm(transformationMatrix * mesh->mVertices[indices.z]);

				aabbMax = glm::max(aabbMax, v0);
				aabbMax = glm::max(aabbMax, v1);
				aabbMax = glm::max(aabbMax, v2);
				aabbMin = glm::min(aabbMin, v0);
				aabbMin = glm::min(aabbMin, v1);
				aabbMin = glm::min(aabbMin, v2);

				auto fastTri = toFast(v0, v1, v2, indices, objectIndex);
				trianglesFirst.push_back(fastTri.firstHalf());
				trianglesSecond.push_back(fastTri.secondHalf());
			}

			uint32_t materialIndex;
			if (sceneMaterialIndices.contains(mesh->mMaterialIndex))
			{
				materialIndex = sceneMaterialIndices[mesh->mMaterialIndex];
			}
			else
			{
				SubmitMaterial(sc->mMaterials[mesh->mMaterialIndex]);
				sceneMaterialIndices.emplace(mesh->mMaterialIndex, materialCursorPos);
				materialIndex = materialCursorPos;
			}

			auto thisEmission = materials[materialIndex].emissive;
			if (thisEmission != glm::vec3(0))
			{
				auto areaSize = glm::distance(aabbMin, aabbMax);
				if (materials[materialIndex].isTexture & 2)
				{
					lights.push_back({
						glm::mix(aabbMin, aabbMax, 0.5f),
						areaSize,
						glm::normalize(averageNormal),
						areaSize * areaSize,
						lightMultiplier * glm::vec4(1),
						objectIndex
						});
				}
				else
				{
					// If this object is emissive, treat is as a light
					Light currentLight = {
						glm::mix(aabbMin, aabbMax, 0.5f),
						areaSize,
						glm::normalize(averageNormal),
						areaSize * areaSize,
						glm::vec4(thisEmission,1.f),
						objectIndex
					};
					lights.push_back(currentLight);
				}
			}
			objects.push_back(SceneObject(
				materialIndex, vboCursorPos, triCursorPos, mesh->mNumFaces,
				mesh->HasVertexColors(0), mesh->HasNormals(), mesh->HasTextureCoords(0)
			));
			std::cout << "is obj number " << objects.size() - 1 << " begins at " << triCursorPos << " with material " << materialIndex << std::endl;
			aiVector3D pos;
			aiQuaternion rot;
			transformationMatrix.DecomposeNoScaling(rot, pos);
			aiVector3D sca = { transformationMatrix[0][0] ,transformationMatrix[1][1],transformationMatrix[2][2] };
			std::cout << "pos " << GlHelpers::aiToGlm(pos) << " rot " << rot.x << "," << rot.y << "," << rot.z << "," << rot.w << " sca " << GlHelpers::aiToGlm(sca) << std::endl;
		}

		// draw all children
		for (auto n = (decltype(nd->mNumChildren))0; n < nd->mNumChildren; ++n)
		{
			SubmitScene(sc, nd->mChildren[n], transformationMatrix);
		}
	}
};