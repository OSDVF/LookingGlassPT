// LookingGlassPT.cpp : Defines the entry point for the application.
//

#include "LookingGlassPT.h"
#include <iostream>
#include <SDL2/SDL.h>
#undef main
#include <GL/glew.h>
#include <cassert>
#include <imgui.h>
#include "impl/imgui_impl_opengl3.h"
#include "impl/imgui_impl_sdl.h"
#include "imgui_internal.h"
#include "ControlWindow.h"
#include "ProjectWindow.h"
#include <thread>
#include <deque>
#define WINDOW_X 500
#define WINDOW_Y 100
#define WINDOW_W 640
#define WINDOW_H 480

int main(int argc, const char** argv)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
	{
		std::cerr << "SDL Init failed" << std::endl;
		return 1;
	}

	/* Request opengl 4.2 context.
	 * SDL doesn't have the ability to choose which profile at this time of writing,
	 * but it should default to the core profile */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	bool forceFlat = false;
	if (argc > 1)
	{
		if (std::string("flat") == argv[1])
		{
			forceFlat = true;
		}
	}
	IMGUI_CHECKVERSION();

	std::array<AppWindow*, 2> windows;
	auto projectWindow = ProjectWindow("Looking Glass Example", WINDOW_X + WINDOW_W, WINDOW_Y, WINDOW_W * 2, WINDOW_H * 2, forceFlat);
	auto controlWindow = ControlWindow("Looking Glass Path Tracer Control", WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H);
	windows[0] = &projectWindow;
	windows[1] = &controlWindow;

	std::array<std::deque<SDL_Event>, windows.size()> eventQueues;

	bool exit = false;
	auto lastTime = SDL_GetPerformanceCounter();
	std::thread renderThread([&windows, &eventQueues, &exit] {
		Helpers::SetThreadName("Drawing Thread");
	for (auto window : windows)
	{
		window->setupGL();
	}
	while (!exit)
	{
		for (int i = 0; i < windows.size(); i++)
		{
			auto& window = windows[i];
			if (window != nullptr)
			{
				auto& events = eventQueues[i];
				// There is not any mutex because it is a non-critical critical section :)
				if (window->close)
				{
					events.clear();
					closeWindow(window);
					window = nullptr;
				}
				else
				{
					window->setContext();
					while (events.size() > 0)
					{
						auto ev = events.front();
						window->eventRender(ev);
						events.pop_front();
					}
					window->draw();
					window->unsetContext();
				}
			}
		}
	}
		}
	);

	// Main thread does the event loop
	Uint32 focusedWindow = -1;
	while (!exit)
	{

		SDL_Event event;
		bool powerSave = true;
		for (auto& window : windows)
		{
			if (window != nullptr)
			{
				powerSave = powerSave && window->powerSave;
			}
		}
		bool hasEvent = powerSave ? SDL_WaitEvent(&event) : SDL_PollEvent(&event);
		auto now = SDL_GetPerformanceCounter();
		float deltaTime = 1000*((now - lastTime) / (float)SDL_GetPerformanceFrequency());
		lastTime = now;
		if (hasEvent)
		{
			std::cout << event.type << std::endl;
			switch (event.type)
			{
			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					focusedWindow = event.window.windowID;
					break;
				case SDL_WINDOWEVENT_TAKE_FOCUS:
					focusedWindow = event.window.windowID;
					break;
				}
				break;
			}
		}
		for (int i = 0; i < windows.size(); i++)
		{
			auto window = windows[i];
			if (window != nullptr)
			{
				if (hasEvent)
				{
					if (window->windowID == focusedWindow ||
						(event.type == SDL_WINDOWEVENT && window->windowID == event.window.windowID))
					{
						eventQueues[i].push_front(event);
						exit = window->eventWork(event, deltaTime);
					}
				}
				else
				{
					// Do working on an empty event
					window->eventWork(event, deltaTime);
				}
			}
		}
	}

	renderThread.join();

	atexit(SDL_Quit);
	return 0;
}

void closeWindow(AppWindow* window)
{
	ImGui::DestroyContext(window->imGuiContext);
	SDL_GL_DeleteContext(window->glContext);
	SDL_DestroyWindow(window->window);
}


