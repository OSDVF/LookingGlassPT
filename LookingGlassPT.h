#pragma once
#include "AppWindow.h"
#include <deque>
#include <SDL.h>

void destroyWindow(AppWindow* window);
bool checkExtensions();
void processEventsOnRender(std::deque<SDL_Event>& events, AppWindow*& window);
