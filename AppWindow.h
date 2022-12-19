#pragma once
#include "PrecompiledHeaders.hpp"
#include <SDL.h>
#include <imgui.h>
#include "impl/imgui_impl_opengl3.h"
#include "impl/imgui_impl_sdl.h"
#include "imgui_internal.h"
#include <GL/glew.h>
#include "Helpers.h"

class AppWindow {
public:
	SDL_Window* window;
	SDL_WindowFlags flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_GLContext glContext;
	ImGuiContext* imGuiContext = nullptr;
	ImGuiIO io;
	float pixelScale;
	float windowWidth;
	float windowHeight;
	float windowPosX;
	float windowPosY;
#ifdef _DEBUG
	bool debugEvents = false;
#endif
	// Event-driven windows are power-saving and are getting renderOnEvent(), other are getting render() every frame
	bool eventDriven = true;
	Uint32 windowID;
	bool destroyMe = false;
	bool hidden = false;
	// Runs on tha main thread
	AppWindow(const char* name, float x, float y, float w, float h);

	// Runs on the render thread
	virtual void setupGL();

	// On render thread
	virtual void resized();

	// On render thread
	virtual void moved();

	// On render thread
	virtual void render();

	void setContext();

	void beginFrame();

	void flushRender();

	// Event handler on the rendering thread
	virtual void renderOnEvent(std::deque<SDL_Event> events);

	void processImGuiEvent(SDL_Event& event);

	// Event handling on worker thread
	virtual bool workOnEvent(SDL_Event event, float deltaTime) = 0;

	void hide();

	void show();

	~AppWindow();
};