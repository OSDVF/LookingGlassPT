#pragma once
#include "AppWindow.h"
#include <deque>
#include <SDL.h>

void closeWindow(AppWindow* window);
void processEventsOnRender(std::deque<SDL_Event>& events, AppWindow*& window);
