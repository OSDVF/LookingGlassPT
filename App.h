#include <SDL.h>
#include "UsbCalibration.h"
#include "BridgeCalibration.h"
#include <fstream>
#include <sstream>
#include <array>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>
#include "GlHelpers.h"
#ifdef WIN32
#include <Windows.h>
#endif


std::filesystem::path relativeToExecutable(std::string filename)
{
	std::filesystem::path executablePath;
#ifdef WIN32
	LPSTR exePath = new CHAR[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	executablePath = std::filesystem::path(exePath);
	delete[] exePath;
#else
	executablePath = std::filesystem::canonical("/proc/self/exe");
#endif

	return executablePath.parent_path() / filename;
}

class App {
public:
	static inline GLuint fShader;
	static inline GLuint vShader;
	static inline GLuint program;
	static inline Calibration calibration;
	static inline GLuint fullScreenVAO;
	static inline GLuint fullScreenVertexBuffer;
	static void setup(ImGuiIO& io)
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
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			std::cerr << "Calibration failed. Using default values." << std::endl;
		}

		GlHelpers::compileShader<GL_VERTEX_SHADER>(relativeToExecutable("vertex.vert").string(), vShader);
		GlHelpers::compileShader<GL_FRAGMENT_SHADER>(relativeToExecutable("fragment.frag").string(), fShader);
		program = glCreateProgram();
		glAttachShader(program, vShader);
		glAttachShader(program, fShader);
		glLinkProgram(program);
		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			std::string infoLog(512, ' ');
			glGetProgramInfoLog(success, 512, NULL, (GLchar*)infoLog.c_str());
			std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
		}
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
	}
	static inline float f;
	static inline int counter = 0;
	static void draw(ImGuiIO& io, SDL_Event event)
	{
		glBindVertexArray(fullScreenVAO);
		glUseProgram(program);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
};