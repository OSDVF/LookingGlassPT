#pragma once
#include "AppWindow.h"
class ControlWindow : public AppWindow {
public:
	using AppWindow::AppWindow;
	void draw() override
	{
		AppWindow::draw();
	}
	bool eventWork(SDL_Event event, float deltaTime) override
	{
		return false;
	}
};