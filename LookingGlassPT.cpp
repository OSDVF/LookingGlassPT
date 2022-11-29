// LookingGlassPT.cpp : Defines the entry point for the application.
//

#include "LookingGlassPT.h"
#include <SDL2/SDL.h>
#undef main
#include <GL/glew.h>
#include <cassert>
#include <imgui.h>
#include "impl/imgui_impl_opengl3.h"
#include "impl/imgui_impl_sdl.h"
#include "imgui_internal.h"
#include "App.h"
#define WINDOW_X 500
#define WINDOW_Y 100
#define WINDOW_W 640
#define WINDOW_H 480
ImGuiIO io;
int main(int argc, const char ** argv)
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

	auto windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	SDL_Window* window = SDL_CreateWindow("Looking Glass Path Tracer Demo", WINDOW_X, WINDOW_Y,
		WINDOW_W, WINDOW_H, windowFlags);

	assert(window);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, context);
	ImGui_ImplOpenGL3_Init("#version 420");

	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return 2;
	}

	bool playing = true;
	bool fullscreen = false;
	bool forceFlat = false;
	bool powerSave = false;
	if (argc > 1)
	{
		if (std::string("flat") == argv[2])
		{
			forceFlat = true;
		}
	}

	App::setup(io, WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, forceFlat);
	while (playing)
	{
		SDL_Event event;
		powerSave ? SDL_WaitEvent(&event) : SDL_PollEvent(&event);

		switch (event.type)
		{
		case SDL_QUIT:
			playing = false;
			break;
		case SDL_MOUSEMOTION:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_MOUSEBUTTONDOWN:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_MOUSEBUTTONUP:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_MOUSEWHEEL:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_KEYUP:
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				playing = 0;
				break;
			case SDLK_f:
				fullscreen = !fullscreen;
				if (fullscreen)
				{
					SDL_SetWindowFullscreen(window, windowFlags | SDL_WINDOW_FULLSCREEN_DESKTOP);
				}
				else
				{
					SDL_SetWindowFullscreen(window, windowFlags);
				}
				break;
			case SDLK_p:
				powerSave = !powerSave;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYDOWN:
			ImGui_ImplSDL2_ProcessEvent(&event);
			break;
		case SDL_WINDOWEVENT:
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.window.event == SDL_WINDOWEVENT_RESIZED)
			{
				float w = event.window.data1;
				float h = event.window.data2;
				glViewport(0, 0, w, h);
			}
			break;
		}
		draw(window, event);

	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	atexit(SDL_Quit);

	return 0;
}

void draw(SDL_Window* window, SDL_Event event)
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	App::draw(io, event);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	SDL_GL_SwapWindow(window);
}