#pragma once
#include <iostream>
#include <SDL.h>
#include <imgui.h>
#include "impl/imgui_impl_opengl3.h"
#include "impl/imgui_impl_sdl.h"
#include "imgui_internal.h"
#include <GL/glew.h>
#include "Helpers.h"
#include <mutex>


class AppWindow {
public:
	SDL_Window* window;
	SDL_WindowFlags flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_GLContext glContext;
	ImGuiContext* imGuiContext;
	ImGuiIO io;
	float pixelScale;
	float windowWidth;
	float windowHeight;
	float windowPosX;
	float windowPosY;
	float powerSave = true;
	Uint32 windowID;
	bool close = false;
	// Runs on tha main thread
	AppWindow(const char* name, float x, float y, float w, float h)
	{
		window = SDL_CreateWindow(name, x, y, w, h, flags);
		windowWidth = w;
		windowHeight = h;
		windowPosX = x;
		windowPosY = y;
		windowID = SDL_GetWindowID(window);
		assert(window);
	}

	// Runs on the render thread
	virtual void setupGL()
	{
		glContext = SDL_GL_CreateContext(window);
		SDL_GL_MakeCurrent(window, glContext);

		glewExperimental = true;
		auto initError = glewInit();
		if (initError != GLEW_OK) {
			std::cerr << "Failed to initialize GLEW" << std::endl;
			std::cerr << glewGetErrorString(initError) << std::endl;
		}

		imGuiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(imGuiContext);
		io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForOpenGL(window, glContext);
		ImGui_ImplOpenGL3_Init("#version 420");

		pixelScale = Helpers::GetVirtualPixelScale(window);
		ImGui::GetStyle().ScaleAllSizes(pixelScale);
		io.FontGlobalScale = pixelScale;
	}

	// On render thread
	virtual void resized()
	{
		glViewport(0, 0, windowWidth, windowHeight);
	}

	// On render thread
	virtual void moved()
	{
	}

	// On render thread
	virtual void draw()
	{
	}

	void setContext()
	{
		if (SDL_GL_MakeCurrent(window, glContext) < 0)
		{
			std::cerr << SDL_GetError() << std::endl;
			SDL_ClearError();
		}
		ImGui::SetCurrentContext(imGuiContext);
		ImGui_ImplSDL2_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();
	}

	void unsetContext()
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	// Event handler on the rendering thread
	virtual void eventRender(SDL_Event event)
	{
		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED:
				windowWidth = event.window.data1;
				windowHeight = event.window.data2;
				resized();
				break;
			case SDL_WINDOWEVENT_MOVED:
				windowPosX = event.window.data1;
				windowPosY = event.window.data2;
				ImGui::GetStyle().ScaleAllSizes(pixelScale);
				io.FontGlobalScale = pixelScale;
				moved();
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_MOUSEBUTTONDOWN:
			ImGui_ImplSDL2_ProcessEvent(&event);
			std::cout << "down" << std::rand() << "\n";
			break;
		case SDL_MOUSEBUTTONUP:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_MOUSEWHEEL:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_KEYUP:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_KEYDOWN:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		}
	}

	// Event handling on worker thread
	virtual bool eventWork(SDL_Event event, float deltaTime) = 0;

	~AppWindow()
	{
		ImGui::SetCurrentContext(imGuiContext);
		// Cleanup
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext(imGuiContext);

		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
	}
};