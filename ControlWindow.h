#pragma once
#include "AppWindow.h"
#include "Settings.h"
#include "UsbCalibration.h"
#include "BridgeCalibration.h"
#include "GlHelpers.h"
#include "alert_exception.h"
#include <imgui.h>
#include <array>
#include <nfd.h>
std::ostream& operator<<(std::ostream& os, const glm::vec3& c)
{
	os << c.r << ',' << c.g << ',' << c.b;
	return os;
}
#define DEBUG_SEVERITY_NOTHING GL_DEBUG_SEVERITY_LOW + 1

// This window is 'lazy' or power-saving, so it doesn't have a draw() method
template <size_t count>
class ControlWindow : public AppWindow {
public:
	std::array<AppWindow*, count>& allWindows;
	std::string calibrationAlert;
	std::string calibratedBy = "Never Calibrated";
	bool debug;
	ControlWindow(std::array<AppWindow*, count>& allWindows, const char* name, float x, float y, float w, float h, bool debug, bool forceFlat = false) :
		AppWindow(name, x, y, w, h), allWindows(allWindows), debug(debug)
	{
		ProjectSettings::fpsWindow = debug;
		if (!debug)
		{
			ProjectSettings::debugOutput = DEBUG_SEVERITY_NOTHING;
		}
		std::cout << "Pixel scale: " << this->pixelScale << std::endl;
		if (forceFlat)
		{
			std::cout << "Forced flat screen." << std::endl;
			ProjectSettings::GlobalScreenType = ProjectSettings::ScreenType::Flat;
		}
		else
		{
			extractCalibration();
		}
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
		ImGui::Begin("Settings", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
		if (debug)
		{
			if (ImGui::TreeNode("Debug"))
			{
				ImGui::Checkbox("Statistics window", &ProjectSettings::fpsWindow);
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
					DEBUG_SEVERITY_NOTHING
				};
				ProjectSettings::debugOutput = indexToSeverity[level];
#ifdef _DEBUG

				if (ImGui::Checkbox("Debug SDL Events", &debugEvents))
				{
					for (auto& win : allWindows)
					{
						win->debugEvents = debugEvents;
					}
				}
#endif
				ImGui::TreePop();
			}
		}
		int step = 1;
		int bigStep = 10; // No step on snek
		if (ImGui::TreeNodeEx("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ProjectSettings::pathTracing)
			{
				if (ImGui::Button("Pause Path Tracing"))
				{
					ProjectSettings::pathTracing = false;
				}
				ImGui::SameLine();
				ImGui::Text("Iteration: %d", ProjectSettings::rayIteration);
			}
			else
			{
				ImGui::InputScalar("Max Iterations", ImGuiDataType_U64, &ProjectSettings::maxIterations, &step, &bigStep);
				if (ImGui::InputScalar("Max Ray Bounces", ImGuiDataType_U64, &ProjectSettings::maxBounces, &step, &bigStep))
				{
					ProjectSettings::recompileFShaders = true;
				}
				ImGui::InputFloat("Ray Offset", &ProjectSettings::rayOffset, 1e-5, 0, "%g");
				if (ImGui::Button("Start/Resume Path Tracing"))
				{
					ProjectSettings::pathTracing = true;
					ProjectSettings::interactive = false;
				}
				if (ProjectSettings::rayIteration > 0)
				{
					if (ImGui::Button("Reset Result"))
					{
						ProjectSettings::rayIteration = 0;
					}
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::RadioButton("Looking Glass", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::LookingGlass))
		{
			ProjectSettings::applyScreenType = true;
		}
		if (ProjectSettings::GlobalScreenType == ProjectSettings::ScreenType::LookingGlass)
		{
			ImGui::TreePush();
			ImGui::Text("Calibrated by: %s", calibratedBy.c_str());
			if (ImGui::Button("Retry Calibration"))
			{
				extractCalibration();
				ProjectSettings::recompileFShaders = true;
			}
			if (ImGui::Checkbox("All subpixels in one pass", &ProjectSettings::subpixelOnePass))
			{
				ProjectSettings::recompileFShaders = true;
			}
			ImGui::TreePop();
		}
		if (ImGui::RadioButton("Flat", (int*)&ProjectSettings::GlobalScreenType, (int)ProjectSettings::ScreenType::Flat))
		{
			ProjectSettings::applyScreenType = true;
		}
		if (ImGui::TreeNode("Camera"))
		{
			bool cameraEdited = ImGui::SliderFloat("FOV", &ProjectSettings::fov, 30.f, 100.f);
			cameraEdited = ImGui::SliderFloat("Near Plane", &ProjectSettings::nearPlane, 0.01f, 1) || cameraEdited;
			cameraEdited = ImGui::SliderFloat("Far Plane", &ProjectSettings::farPlane, ProjectSettings::nearPlane, 1000) || cameraEdited;
			ImGui::SliderFloat("Sensitivity", &ProjectSettings::person.Camera.Sensitivity, 0.01, 1);
			ImGui::InputFloat3("P", glm::value_ptr(ProjectSettings::person.Camera._position));
			ImGui::InputFloat3("R", glm::value_ptr(ProjectSettings::person.Camera._rotation));
			ImGui::TreePop();

			if (cameraEdited)
			{
				ProjectSettings::person.Camera.SetProjectionMatrixPerspective(
					ProjectSettings::fov,
					ProjectSettings::person.Camera.Aspect,
					ProjectSettings::nearPlane,
					ProjectSettings::farPlane
				);
			}
		}
		bool headerDrawn = false;
		for (int i = 0; i < allWindows.size(); i++)
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
					//std::cout << GlHelpers::aiToGlm(ProjectSettings::scene.scale) << std::endl;
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
			ImGui::SliderFloat("Light Multiplier", &ProjectSettings::lightMultiplier, 0.1, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
			if (ImGui::Checkbox("Backface Culling", &ProjectSettings::backfaceCulling))
			{
				ProjectSettings::recompileFShaders = true;
			}

			ImGui::InputScalar("Maximum Objects", ImGuiDataType_U32, &ProjectSettings::objectCountLimit, &step);
			if (ImGui::Button("(Re)load"))
			{
				ProjectSettings::reloadScene = true;
			}
			ImGui::TreePop();
		}


		const char* calibrationAlertText = "Calibration Warning";
		if (!calibrationAlert.empty())
		{
			ImGui::OpenPopup(calibrationAlertText);
		}
		if (ImGui::BeginPopup(calibrationAlertText))
		{
			ImGui::TextWrapped(calibrationAlert.c_str());
			if (ImGui::Button("Close"))
			{
				calibrationAlert.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
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

	/**
	* Sets calibration and ScreenType
	*/
	void extractCalibration()
	{
		ProjectSettings::GlobalScreenType = ProjectSettings::ScreenType::Flat;
		try {
			try {
				std::cout << "Trying Looking Glass Bridge calibration..." << std::endl;
				ProjectSettings::calibration = BridgeCalibration().getCalibration();
				calibratedBy = "Looking Glass Bridge";
			}
			catch (const std::runtime_error& e)
			{
				std::cout << "Trying USB calibration..." << std::endl;
				ProjectSettings::calibration = UsbCalibration().getCalibration();
				calibratedBy = "USB Interface";
				std::cerr << e.what() << std::endl;
			}
			ProjectSettings::GlobalScreenType = ProjectSettings::ScreenType::LookingGlass;
			std::cout << "Calibration success: " << std::endl << ProjectSettings::calibration;
		}
		catch (const alert_exception& e)
		{
			calibrationAlert = e.what();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			std::cerr << "Calibration failed. Displaying flat screen version. LG version will use default values." << std::endl;
			calibratedBy = "Error";
		}
	}
};