// LookingGlassPT.cpp : Defines the entry point for the application.
//

#include "LookingGlassPT.h"
#include <SDL2/SDL.h>
#undef main
#include <GL/glew.h>
#include <cassert>

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

	/* Create our window centered at 512x512 resolution */
	SDL_Window* window = SDL_CreateWindow("Looking Glass Path Tracer Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	assert(window);
	SDL_GLContext context = SDL_GL_CreateContext(window);

	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return 2;
	}

	bool playing = true;
	while (playing)
	{
		SDL_Event event;
		if (SDL_WaitEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				playing = false;
				break;
			case SDL_MOUSEMOTION:
				draw(window);
				break;
			case SDL_MOUSEBUTTONDOWN:
				draw(window);
				break;
			case SDL_MOUSEBUTTONUP:
				draw(window);
				break;
			case SDL_MOUSEWHEEL:
				draw(window);
				break;
			case SDL_KEYUP: /* fallthrough */
			case SDL_KEYDOWN:
				printf("`%c' was %s\n",
					event.key.keysym.sym,
					(event.key.state == SDL_PRESSED) ? "pressed" : "released");
				break;
			}
		}
		else
		{
			std::cerr << SDL_GetError() << std::endl;
		}
	}
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	atexit(SDL_Quit);

	return 0;
}


void draw(SDL_Window* window)
{
	glClearColor(0.0, 1.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
}