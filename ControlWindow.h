#pragma once
#include "AppWindow.h"
#include "Settings.h"
#include "GlHelpers.h"
#include <imgui.h>
#include <array>
#include <nfd.h>

// This window is 'lazy' or power-saving, so it doesn't have a draw() method
template <size_t count>
class ControlWindow : public AppWindow {
public:
	std::array<AppWindow*, count>& allWindows;
	bool debug;
	ControlWindow(std::array<AppWindow*, count>& allWindows, const char* name, float x, float y, float w, float h, bool debug) :
		AppWindow(name, x, y, w, h), allWindows(allWindows), debug(debug)
	{

	}

	bool logarithmicScale = true;
	bool uniformScale = true;
	const char* fileErrorDialog = "File Selection Error";
	// Redraws only when a event occurs
	void renderOnEvent(std::deque<SDL_Event> events) override
	{
		AppWindow::renderOnEvent(events);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));
		ImGui::Begin("Settings", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);                          // Create a window called "Hello, world!" and append into it.
		if (debug)
		{
			const char* const severities[] = {
				"Everything",
				"Low or higher",
				"Medium or high",
				"High",
				"None"
			};
			int level;
			switch (ProjectSettings::debugOutput)
			{
			case GL_DEBUG_SEVERITY_NOTIFICATION:
				level = 0;
				break;
			case GL_DEBUG_SEVERITY_LOW:
				level = 1;
				break;
			case GL_DEBUG_SEVERITY_MEDIUM:
				level = 2;
				break;
			case GL_DEBUG_SEVERITY_HIGH:
				level = 3;
				break;
			default:
				level = 4;
			}
			ImGui::Combo("GL Debug Level", &level, severities, 5);
			GLenum indexToSeverity[] = { GL_DEBUG_SEVERITY_NOTIFICATION,
				GL_DEBUG_SEVERITY_LOW,
				GL_DEBUG_SEVERITY_MEDIUM,
				GL_DEBUG_SEVERITY_HIGH,
				GL_DEBUG_SEVERITY_LOW + 1
			};
			ProjectSettings::debugOutput = indexToSeverity[level];
		}
		if (ImGui::RadioButton("Looking Glass", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::LookingGlass))
		{
			ProjectSettings::applyScreenType = true;
		}
		if (ImGui::RadioButton("Flat", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::Flat))
		{
			ProjectSettings::applyScreenType = true;
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
		if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ProjectSettings::scene.path.empty())
			{
				ImGui::Text("No Scene Loaded");
			}
			else
			{
				ImGui::Text("%s", ProjectSettings::scene.path.filename().string().c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button("Select"))
			{
				nfdchar_t* outPath = nullptr;
				nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);

				if (result == NFD_OKAY) {
					std::cout << "Selected " << outPath << std::endl;
					ProjectSettings::scene.path = outPath;
					free(outPath);
				}
				else if (result == NFD_CANCEL) {
					std::cout << "Scene file section cancelled.\n";
				}
				else {
					ImGui::OpenPopup(fileErrorDialog);
				}
			}
			if (ImGui::BeginPopup(fileErrorDialog))
			{
				ImGui::TextWrapped("Error: %s", NFD_GetError());
				if (ImGui::Button("Close"))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			if (logarithmicScale)
			{
				auto scPowerText = "Scale Power";
				glm::vec3 scalePower = { log10(ProjectSettings::scene.scale.x), log10(ProjectSettings::scene.scale.y), log10(ProjectSettings::scene.scale.z) };
				if (uniformScale ? ImGui::DragFloat(scPowerText, glm::value_ptr(scalePower), .1f, -5, 5) :
					ImGui::DragFloat3(scPowerText, glm::value_ptr(scalePower), .1f, -5, 5))
				{
					if (uniformScale)
					{
						scalePower.y = scalePower.z = scalePower.x;
					}
					ProjectSettings::scene.scale = GlHelpers::structConvert<aiVector3D, glm::vec3>(glm::pow(glm::vec3(10.f), scalePower));
				}
			}
			else
			{
				if (uniformScale)
				{
					ImGui::DragFloat("Scale##Scene", &ProjectSettings::scene.scale.x, 100.f, 0.00001, 100000, "%f", ImGuiSliderFlags_Logarithmic);
					ProjectSettings::scene.scale.y = ProjectSettings::scene.scale.z = ProjectSettings::scene.scale.x;
				}
				else
				{
					ImGui::DragFloat3("Scale##Scene", &ProjectSettings::scene.scale.x, 100.f, 0.00001, 100000, "%f", ImGuiSliderFlags_Logarithmic);
				}
			}
			ImGui::TreePush();
			ImGui::Checkbox("Logarithmic Scale", &logarithmicScale);
			ImGui::SameLine();
			ImGui::Checkbox("Uniform", &uniformScale);
			ImGui::TreePop();
			ImGui::DragFloat3("Position##Scene", &ProjectSettings::scene.position.x, .1f, -10000, 10000);
			ImGui::DragFloat3("Rotation (deg)", &ProjectSettings::scene.rotationDeg.x, .1f, 0.f, 360.f - FLT_EPSILON);

			if (ImGui::Button("(Re)load"))
			{
				ProjectSettings::reloadScene = true;
			}
			ImGui::TreePop();
			int step = 1;
			ImGui::InputScalar("Maximum Objects", ImGuiDataType_U32, &ProjectSettings::objectCountLimit, &step);
		}

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