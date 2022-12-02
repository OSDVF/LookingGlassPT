#pragma once
#include "AppWindow.h"
#include <deque>
#include <SDL.h>

void destroyWindow(AppWindow* window);
void processEventsOnRender(std::deque<SDL_Event>& events, AppWindow*& window);
