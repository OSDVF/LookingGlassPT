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
#include "App.h"

ImGuiIO io;
int main()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
	{
		std::cerr << "SDL Init failed" << std::endl;
		return 1;
	}

	/* Request opengl 3.2 context.
	 * SDL doesn't have the ability to choose which profile at this time of writing,
	 * but it should default to the core profile */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	/* Turn on double buffering with a 24bit Z buffer.
	 * You may need to change this to 16 or 32 for your system */
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	auto windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	SDL_Window* window = SDL_CreateWindow("Looking Glass Path Tracer Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		640, 480, windowFlags);

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
	ImGui_ImplOpenGL3_Init("#version 430");

	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return 2;
	}

	bool playing = true;
	bool fullscreen = false;

	App::setup(io);
	while (playing)
	{
		SDL_Event event;
		if (SDL_WaitEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.type)
			{
			case SDL_QUIT:
				playing = false;
				break;
			case SDL_MOUSEMOTION:
				draw(window, event);
				break;
			case SDL_MOUSEBUTTONDOWN:
				draw(window, event);
				break;
			case SDL_MOUSEBUTTONUP:
				draw(window, event);
				break;
			case SDL_MOUSEWHEEL:
				draw(window, event);
				break;
			case SDL_KEYUP: /* fallthrough */
				switch (event.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					playing = 0;
					break;
				case 'f':
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
				default:
					break;
				}
			case SDL_KEYDOWN:
				printf("`%c' was %s\n",
					event.key.keysym.sym,
					(event.key.state == SDL_PRESSED) ? "pressed" : "released");
				draw(window, event);
				break;
			}
		}
		else
		{
			std::cerr << SDL_GetError() << std::endl;
		}
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
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	glClearColor(0.0, 1.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	SDL_GL_SwapWindow(window);
}