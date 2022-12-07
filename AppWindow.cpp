#include "AppWindow.h"

AppWindow::AppWindow(const char* name, float x, float y, float w, float h)
{
	window = SDL_CreateWindow(name, x, y, w, h, flags);
	windowWidth = w;
	windowHeight = h;
	windowPosX = x;
	windowPosY = y;
	windowID = SDL_GetWindowID(window);
	pixelScale = Helpers::GetVirtualPixelScale(window);
	assert(window);
}

// Runs on the render thread
void AppWindow::setupGL()
{
	glContext = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, glContext);

	glewExperimental = true;
	auto initError = glewInit();
	if (initError != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		std::cerr << glewGetErrorString(initError) << std::endl;
	}

	auto tempImCtx = ImGui::CreateContext();
	ImGui::SetCurrentContext(tempImCtx);
	io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, glContext);
	ImGui_ImplOpenGL3_Init("#version 420");

	ImGui::GetStyle().ScaleAllSizes(pixelScale);
	io.FontGlobalScale = pixelScale;
	imGuiContext = tempImCtx;
}

// On render thread
void AppWindow::resized()
{
	glViewport(0, 0, windowWidth, windowHeight);
}

// On render thread
void AppWindow::moved()
{
}

// On render thread
void AppWindow::render()
{
}

void AppWindow::setContext()
{
	if (SDL_GL_MakeCurrent(window, glContext) < 0)
	{
		std::cerr << SDL_GetError() << std::endl;
		SDL_ClearError();
	}
	ImGui::SetCurrentContext(imGuiContext);
}

void AppWindow::beginFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void AppWindow::flushRender()
{
	bool debug = glIsEnabled(GL_DEBUG_OUTPUT);
	if (debug)
	{
		glDisable(GL_DEBUG_OUTPUT);
	}
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window);
	if (debug)
	{
		glEnable(GL_DEBUG_OUTPUT);
	}
}

// Event handler on the rendering thread
void AppWindow::renderOnEvent(std::deque<SDL_Event> events)
{
	for (auto& event : events)
	{
		processImGuiEvent(event);
	}
	beginFrame();
}

void AppWindow::processImGuiEvent(SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);
	switch (event.type)
	{
	case SDL_WINDOWEVENT:
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
			pixelScale = Helpers::GetVirtualPixelScale(window);
			ImGui::GetStyle().ScaleAllSizes(pixelScale);
			io.FontGlobalScale = pixelScale;
			moved();
			break;
		}
		break;
	}
}


void AppWindow::hide()
{
	if (!hidden)
	{
		SDL_CaptureMouse(SDL_FALSE);
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_HideWindow(window);
		hidden = true;
	}
}

void AppWindow::show()
{
	if (hidden)
	{
		SDL_ShowWindow(window);
		hidden = false;
	}
}

AppWindow::~AppWindow()
{
	ImGui::SetCurrentContext(imGuiContext);
	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext(imGuiContext);

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(window);
}