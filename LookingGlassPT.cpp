// LookingGlassPT.cpp : Defines the entry point for the application.
//
#include "PrecompiledHeaders.hpp"
#ifdef WIN32
#include <Windows.h>
#endif
#include "LookingGlassPT.h"
#include <SDL2/SDL.h>
#undef main
#include <GL/glew.h>
#include <cassert>
#include <imgui.h>
#include "impl/imgui_impl_opengl3.h"
#include "impl/imgui_impl_sdl.h"
#include "impl/sdl_event_to_string.h"
#include "imgui_internal.h"
#include "Window/ControlWindow.h"
#include "Window/ProjectWindow.h"

#define WINDOW_X 5
#define WINDOW_Y 100
#define WINDOW_W 640
#define WINDOW_H 600

std::mutex queMut;
std::mutex powerSaveMut;
std::condition_variable renderWait;
bool wholeAppPowerSave = false;
int main(int argc, const char** argv)
{
	float performanceFrequency = SDL_GetPerformanceFrequency();
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
	bool debug = false;
	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			if (std::string("flat") == argv[i])
			{
				forceFlat = true;
			}
			else if (std::string("d") == argv[i])
			{
				debug = true;
			}
		}
	}
	if (!debug)
	{
#ifdef WIN32
		FreeConsole();
#endif
	}
	IMGUI_CHECKVERSION();

	std::array<AppWindow*, 2> windows = {
	new ProjectWindow("Looking Glass Example", WINDOW_X + WINDOW_W, WINDOW_Y, WINDOW_W * 2, WINDOW_H * 1.5),
	new ControlWindow(windows, "Looking Glass Path Tracer Control", WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, debug, forceFlat),
	};

	std::array<std::deque<SDL_Event>, windows.size()> eventQueues;

	bool exit = false;
	float lastTime = SDL_GetPerformanceCounter();
	std::thread renderThread([&windows, &eventQueues, &exit, debug] {
		Helpers::SetThreadName("Drawing Thread");
		for (auto window : windows)
		{
			window->setupGL();
			if(debug)
			{
				// Disable vsync
				SDL_GL_SetSwapInterval(0);
			}
		}
		if (!checkExtensions())
		{
			return;
		}
		while (!exit)
		{
			for (int i = 0; i < windows.size(); i++)
			{
				auto& window = windows[i];
				if (window != nullptr)
				{
					if (!window->hidden)
					{
						auto& events = eventQueues[i];
						// There is not any mutex because it is a non-critical critical section :)
						if (window->destroyMe)
						{
							events.clear();
							destroyWindow(window);
							window = nullptr;
						}
						else
						{
							if (window->eventDriven && !SceneAndViewSettings::overridePowerSave)
							{
								if (events.size())
								{
									// Process events
									window->setContext();
									window->beginFrame();
									processEventsOnRender(events, window);
									window->flushRender();

									// Render once more with empty event to update the UI
									window->setContext();
									window->beginFrame();
									window->renderOnEvent(events);
									window->flushRender();
								}
								else if (wholeAppPowerSave)
								{
									std::unique_lock lk(powerSaveMut);
									renderWait.wait(lk);
								}
							}
							else
							{
								window->setContext();
								while (events.size() > 0)
								{
									window->processImGuiEvent(events.front());
									events.pop_front();
								}
								window->beginFrame();
								window->render();
								window->flushRender();
							}
						}
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
		bool tempPowerSaveResult = true;
		for (auto& window : windows)
		{
			if (window != nullptr)
			{
				tempPowerSaveResult = tempPowerSaveResult && (window->eventDriven || window->hidden);
			}
		}
		wholeAppPowerSave = tempPowerSaveResult;
		bool hasEvent;
		if (wholeAppPowerSave)
		{
			hasEvent = SDL_WaitEvent(&event);
			renderWait.notify_all();
		}
		else
		{
			hasEvent = SDL_PollEvent(&event);
		}

		float now = SDL_GetPerformanceCounter();
		float deltaTime = std::clamp((now - lastTime) / performanceFrequency, 0.001f, 1.f);
		lastTime = now;
		if (hasEvent)
		{
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
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				focusedWindow = event.button.windowID;
				break;
			case SDL_MOUSEMOTION:
				focusedWindow = event.motion.windowID;
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				focusedWindow = event.key.windowID;
			case SDL_MOUSEWHEEL:
				focusedWindow = event.wheel.windowID;
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
						try
						{
							std::unique_lock lck(queMut);
							eventQueues[i].push_back(event);
						}
						catch (std::exception)
						{
							//Do nothing, probably a race condition occured when accessing event queue
						}
						exit = exit || window->workOnEvent(event, deltaTime);
					}
				}
				else
				{
					// Do working on an empty event
					window->workOnEvent(event, deltaTime);
					std::this_thread::yield();
				}
			}
		}// for all windows
	}// while (!exit)

	renderWait.notify_all();
	renderThread.join();

	atexit(SDL_Quit);
	for (auto window : windows)
	{
		delete window;
	}
	return 0;
}

// Read dispatched events on render thread
void processEventsOnRender(std::deque<SDL_Event>& events, AppWindow*& window)
{
	std::unique_lock lck(queMut, std::defer_lock);
	if (!window->hidden)
	{
		lck.lock();
	}
	std::deque<SDL_Event> eventsCopy = events;
	lck.unlock();
	window->renderOnEvent(eventsCopy);
	lck.lock();
	events.clear();
}

void destroyWindow(AppWindow* window)
{
	ImGui::DestroyContext(window->imGuiContext);
	SDL_GL_DeleteContext(window->glContext);
	SDL_DestroyWindow(window->window);
}

bool checkExtensions()
{
	if (!GLEW_ARB_bindless_texture)
	{
		std::cerr << "Bindless textures support is required.\n";
		std::cout << "Available extensions:\n";
		GLint n = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &n);

		for (GLint i = 0; i < n; i++)
		{
			const char* extension =
				(const char*)glGetStringi(GL_EXTENSIONS, i);
			std::cout << fmt::format("{}: {}\n", i, extension) << std::endl;
		}
		return false;
	}
	return true;
}

