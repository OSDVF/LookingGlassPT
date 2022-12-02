#pragma once
#include "AppWindow.h"
#include "Settings.h"
#include <imgui.h>
#include <array>
#include <glm/gtc/type_ptr.hpp>

// This window is 'lazy' or power-saving, so it doesn't have a draw() method
template <size_t count>
class ControlWindow : public AppWindow {
public:
	std::array<AppWindow*, count>& allWindows;
	ControlWindow(std::array<AppWindow*, count>& allWindows, const char* name, float x, float y, float w, float h) :
		AppWindow(name, x, y, w, h), allWindows(allWindows)
	{

	}
	// Redraws only when a event occurs
	void renderOnEvent(std::deque<SDL_Event> events) override
	{
		AppWindow::renderOnEvent(events);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
		ImGui::Begin("Settings", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);                          // Create a window called "Hello, world!" and append into it.
		if (ImGui::RadioButton("Looking Glass", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::LookingGlass))
		{
			ProjectSettings::changeScreenType = true;
		}
		if (ImGui::RadioButton("Flat", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::Flat))
		{
			ProjectSettings::changeScreenType = true;
		}
		bool cameraEdited = ImGui::SliderFloat("FOV", &ProjectSettings::fov, 30.f, 100.f);
		cameraEdited = ImGui::SliderFloat("Near Plane", &ProjectSettings::nearPlane, 0.01f, 1) || cameraEdited;
		cameraEdited = ImGui::SliderFloat("Far Plane", &ProjectSettings::farPlane, ProjectSettings::nearPlane, 1000) || cameraEdited;
		ImGui::SliderFloat("Sensitivity", &ProjectSettings::person.Camera.Sensitivity, 0.01, 1);
		ImGui::InputFloat3("P", glm::value_ptr(ProjectSettings::person.Camera._position));
		ImGui::InputFloat3("R", glm::value_ptr(ProjectSettings::person.Camera._rotation));
		if (cameraEdited)
		{
			ProjectSettings::person.Camera.SetProjectionMatrixPerspective(
				ProjectSettings::fov,
				ProjectSettings::person.Camera.Aspect,
				ProjectSettings::nearPlane,
				ProjectSettings::farPlane
			);
		}
		bool headerDrawn = false;
		for (int i = 1; i < allWindows.size(); i++)
		{
			auto& window = allWindows[i];
			if (window != nullptr && window->hidden)
			{
				if (!headerDrawn)
				{
					ImGui::Text("Show Windows");
					headerDrawn = true;
				}
				if (i > 1)
				{
					ImGui::SameLine();
				}
				if (ImGui::Button(std::format("W {}", i).c_str()))
				{
					window->show();
				}
			}
		}
		ImGui::SliderFloat("View Cone", &ProjectSettings::viewCone, 10.f, 80.f);
		ImGui::SliderFloat("Focus Distance", &ProjectSettings::focusDistance, 0.f, 40.f);

		ImGui::End();
	}
	bool workOnEvent(SDL_Event event, float deltaTime) override
	{
		switch (event.type)
		{
		case SDL_EventType::SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_CLOSE:
				return true;
			}
			break;
		}
		return false;
	}
};