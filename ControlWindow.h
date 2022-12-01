#pragma once
#include "AppWindow.h"
#include "Settings.h"
#include <imgui.h>

// This window is 'lazy' or power-saving, so it doesn't have a draw() method
class ControlWindow : public AppWindow {
public:
	using AppWindow::AppWindow;
	// Redraws only when a event occurs
	void eventRender(std::deque<SDL_Event>& events) override
	{
		AppWindow::eventRender(events);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
		ImGui::Begin("Settings", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);                          // Create a window called "Hello, world!" and append into it.
		if (ImGui::RadioButton("Looking Glass", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::LookingGlass))
		{
			ProjectSettings::swapShaders = true;
		}
		if (ImGui::RadioButton("Flat", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::Flat))
		{
			ProjectSettings::swapShaders = true;
		}
		bool cameraEdited = ImGui::SliderFloat("FOV", &ProjectSettings::fov, 30.f, 100.f);
		cameraEdited = ImGui::SliderFloat("Near Plane", &ProjectSettings::nearPlane, 0.01f, 1) || cameraEdited;
		cameraEdited = ImGui::SliderFloat("Far Plane", &ProjectSettings::farPlane, ProjectSettings::nearPlane, 1000) || cameraEdited;
		if (cameraEdited)
		{
			ProjectSettings::person.Camera.SetProjectionMatrixPerspective(
				ProjectSettings::fov,
				ProjectSettings::person.Camera.Aspect,
				ProjectSettings::nearPlane,
				ProjectSettings::farPlane
			);
		}
		ImGui::SliderFloat("View Cone", &ProjectSettings::viewCone, 10.f, 80.f);
		ImGui::SliderFloat("Focus Distance", &ProjectSettings::focusDistance, 0.f, 40.f);

		ImGui::End();
	}
	bool eventWork(SDL_Event event, float deltaTime) override
	{
		return false;
	}
};