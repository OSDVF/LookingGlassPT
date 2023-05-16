#pragma once
#include "AppWindow.h"
#include "../Structures/SceneAndViewSettings.h"
#include "../Structures/AlertException.h"
#include "../Calibration/UsbCalibration.h"
#include "../Calibration/BridgeCalibration.h"
#include "../GlHelpers.h"
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
		SceneAndViewSettings::fpsWindow = debug;
		if (!debug)
		{
			SceneAndViewSettings::debugOutput = DEBUG_SEVERITY_NOTHING;
		}
		std::cout << "Pixel scale: " << this->pixelScale << std::endl;
		if (forceFlat)
		{
			std::cout << "Forced flat screen." << std::endl;
			SceneAndViewSettings::GlobalScreenType = SceneAndViewSettings::ScreenType::Flat;
		}
		else
		{
			extractCalibration();
		}
	}

	bool logarithmicScale = false;
	bool uniformScale = true;
	const char* fileErrorDialog = "File Selection Error";
	int bvhVisualizeIterations = 5;
	std::chrono::high_resolution_clock::time_point pathTracingFirstFrame;
	long pathTracingDuration = -1;
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
				ImGui::Checkbox("Statistics window", &SceneAndViewSettings::fpsWindow);
				if (ImGui::Checkbox("Visualize BVH", &SceneAndViewSettings::visualizeBVH))
				{
					SceneAndViewSettings::recompileFShaders = true;
				}
				if (SceneAndViewSettings::visualizeBVH)
				{
					ImGui::TreePush("BVH Iterations");
					ImGui::InputInt("Iterations", &bvhVisualizeIterations);
					for (int i = 0; i < bvhVisualizeIterations; i++)
					{
						char name[] = "Level   ";
						name[7] = '0' + i % 10;
						name[6] = '0' + i / 10;
						bool checked = (SceneAndViewSettings::bvhDebugIterationsMask & (1 << i)) != 0;
						if (ImGui::Checkbox(name, &checked))
						{
							if (checked)
							{
								SceneAndViewSettings::bvhDebugIterationsMask |= (1 << i);
							}
							else
							{
								SceneAndViewSettings::bvhDebugIterationsMask &= ~(1 << i);
							}
							SceneAndViewSettings::recompileFShaders = true;
						}
					}
					ImGui::TreePop();
				}

				const char* const severities[] = {
					"Everything",
					"Low or higher",
					"Medium or high",
					"High",
					"None"
				};
				int level;
				switch (SceneAndViewSettings::debugOutput)
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
				SceneAndViewSettings::debugOutput = indexToSeverity[level];
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
		constexpr enum ImGuiDataType_ dataType = sizeof(SceneAndViewSettings::scene.scale.x) == sizeof(float) ? ImGuiDataType_Float : ImGuiDataType_Double;
		if (ImGui::TreeNodeEx("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (SceneAndViewSettings::pathTracing)
			{
				if (ImGui::Button("Pause Path Tracing"))
				{
					SceneAndViewSettings::stopPathTracing();
				}
				ImGui::SameLine();
				ImGui::Text("Iteration: %lu", SceneAndViewSettings::rayIteration);
			}
			else
			{
				if(pathTracingDuration == 0)
				{
					pathTracingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - pathTracingFirstFrame).count();
				}
				if(pathTracingDuration > 0)
				{
					ImGui::Text("Took %ld ms", pathTracingDuration);
				}
				ImGui::InputScalar("Max Iterations", ImGuiDataType_U64, &SceneAndViewSettings::maxIterations, &step, &bigStep);
				if (ImGui::InputScalar("Max Ray Bounces", ImGuiDataType_U64, &SceneAndViewSettings::maxBounces, &step, &bigStep))
				{
					SceneAndViewSettings::recompileFShaders = true;
				}
				ImGui::InputFloat("Ray Offset", &SceneAndViewSettings::rayOffset, 1e-5, 0, "%g");
				if (ImGui::Button("Start/Resume Path Tracing"))
				{
					SceneAndViewSettings::startPathTracing();
					pathTracingFirstFrame = std::chrono::high_resolution_clock::now();
					pathTracingDuration = 0;
				}
				if (SceneAndViewSettings::rayIteration > 0)
				{
					if (ImGui::Button("Reset Result"))
					{
						SceneAndViewSettings::rayIteration = 0;
						pathTracingDuration = -1;
					}
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::RadioButton("Looking Glass", (int*)&SceneAndViewSettings::GlobalScreenType, (int)SceneAndViewSettings::ScreenType::LookingGlass))
		{
			SceneAndViewSettings::applyScreenType = true;
		}
		if (SceneAndViewSettings::GlobalScreenType == SceneAndViewSettings::ScreenType::LookingGlass)
		{
			ImGui::TreePush("LG specific");
			ImGui::SliderFloat("View Cone", &SceneAndViewSettings::viewCone, 10.f, 80.f);
			ImGui::SliderFloat("Focus Distance", &SceneAndViewSettings::focusDistance, 0.f, 40.f);
			ImGui::Text("Calibrated by: %s", calibratedBy.c_str());
			if (ImGui::Button("Retry Calibration"))
			{
				extractCalibration();
				SceneAndViewSettings::recompileFShaders = true;
			}
			if (ImGui::Checkbox("All subpixels in one pass", &SceneAndViewSettings::subpixelOnePass))
			{
				SceneAndViewSettings::recompileFShaders = true;
			}
			ImGui::TreePop();
		}
		if (ImGui::RadioButton("Flat", (int*)&SceneAndViewSettings::GlobalScreenType, (int)SceneAndViewSettings::ScreenType::Flat))
		{
			SceneAndViewSettings::applyScreenType = true;
		}
		if (ImGui::TreeNode("Camera and movement"))
		{
			ImGui::SliderFloat("Sensitivity", &SceneAndViewSettings::person.Camera.Sensitivity, 1, 500);
			if (ImGui::SliderFloat("Speed", &SceneAndViewSettings::person.WalkSpeed, 0.01, 5))
			{
				SceneAndViewSettings::person.RunSpeed = SceneAndViewSettings::person.WalkSpeed * 5;
			}
			bool cameraEdited = ImGui::SliderFloat("FOV", &SceneAndViewSettings::fov, 30.f, 100.f);
			cameraEdited = ImGui::SliderFloat("Near Plane", &SceneAndViewSettings::nearPlane, 0.01f, 1) || cameraEdited;
			cameraEdited = ImGui::SliderFloat("Far Plane", &SceneAndViewSettings::farPlane, SceneAndViewSettings::nearPlane, 1000) || cameraEdited;
			ImGui::InputFloat3("Pos", glm::value_ptr(SceneAndViewSettings::person.Camera._position));
			ImGui::InputFloat3("Rot", glm::value_ptr(SceneAndViewSettings::person.Camera._rotation));
			ImGui::TreePop();

			if (cameraEdited)
			{
				SceneAndViewSettings::person.Camera.SetProjectionMatrixPerspective(
					SceneAndViewSettings::fov,
					SceneAndViewSettings::person.Camera.Aspect,
					SceneAndViewSettings::nearPlane,
					SceneAndViewSettings::farPlane
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
				if (ImGui::Button(fmt::format("W {}", i).c_str()))
				{
					window->show();
				}
			}
		}
		if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (SceneAndViewSettings::scene.path.empty())
			{
				ImGui::Text("No Scene Loaded");
			}
			else
			{
				ImGui::Text("%s", SceneAndViewSettings::scene.path.filename().string().c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button("Select"))
			{
				nfdchar_t* outPath = nullptr;
				nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);

				if (result == NFD_OKAY) {
					std::cout << "Selected " << outPath << std::endl;
					SceneAndViewSettings::scene.path = outPath;
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
				glm::vec3 scalePower = { log10(SceneAndViewSettings::scene.scale.x), log10(SceneAndViewSettings::scene.scale.y), log10(SceneAndViewSettings::scene.scale.z) };
				const decltype(SceneAndViewSettings::scene.scale.x) min = -5;
				const decltype(SceneAndViewSettings::scene.scale.x) max = 5;
				if (uniformScale ? ImGui::DragScalar(scPowerText, dataType, glm::value_ptr(scalePower), .1f, &min, &max) :
					ImGui::DragScalarN(scPowerText, dataType, glm::value_ptr(scalePower), 3, .1f, &min, &max))
				{
					if (uniformScale)
					{
						scalePower.y = scalePower.z = scalePower.x;
					}
					SceneAndViewSettings::scene.scale = GlHelpers::structConvert<aiVector3D, glm::vec3>(glm::pow(glm::vec3(10.f), scalePower));
					//std::cout << GlHelpers::aiToGlm(SceneAndViewSettings::scene.scale) << std::endl;
				}
			}
			else
			{
				const decltype(SceneAndViewSettings::scene.scale.x) min = 0.00001;
				const decltype(SceneAndViewSettings::scene.scale.x) max = 100000;
				if (uniformScale)
				{
					ImGui::DragScalar("Scale##Scene", dataType, &SceneAndViewSettings::scene.scale.x, 100.f, &min, &max, "%lf", ImGuiSliderFlags_Logarithmic);
					SceneAndViewSettings::scene.scale.y = SceneAndViewSettings::scene.scale.z = SceneAndViewSettings::scene.scale.x;
				}
				else
				{
					ImGui::DragScalarN("Scale##Scene", dataType, &SceneAndViewSettings::scene.scale.x, 3, 100.f, &min, &max, "%f", ImGuiSliderFlags_Logarithmic);
				}
			}
			ImGui::TreePush("Details");
			ImGui::Checkbox("Logarithmic Scale", &logarithmicScale);
			ImGui::SameLine();
			ImGui::Checkbox("Uniform", &uniformScale);
			ImGui::TreePop();
			decltype(SceneAndViewSettings::scene.position.x) min = -10000;
			decltype(SceneAndViewSettings::scene.position.x) max = -10000;
			ImGui::DragScalarN("Position##Scene", dataType, &SceneAndViewSettings::scene.position.x, 3, .1f, &min, &max);
			min = 0;
			max = 360 - (dataType == ImGuiDataType_::ImGuiDataType_Float ? FLT_EPSILON : DBL_EPSILON);
			ImGui::DragScalarN("Rotation (deg)", dataType, &SceneAndViewSettings::scene.rotationDeg.x, 3, .1f, &min, &max);
			ImGui::SliderFloat("Light Multiplier", &SceneAndViewSettings::lightMultiplier, 0.1, 10.f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::Checkbox("Skylight", &SceneAndViewSettings::skyLight);
			if (ImGui::Checkbox("Backface Culling", &SceneAndViewSettings::backfaceCulling))
			{
				SceneAndViewSettings::recompileFShaders = true;
			}

			if (ImGui::TreeNode("Performance"))
			{
				ImGui::InputScalar("Max Triangles For SAH", ImGuiDataType_U32, &SceneAndViewSettings::bvhSAHthreshold, &step, &bigStep);
				ImGui::InputScalar("Maximum Objects", ImGuiDataType_U32, &SceneAndViewSettings::objectCountLimit, &step);
				ImGui::TreePop();
			}
			if (ImGui::Button("(Re)load"))
			{
				SceneAndViewSettings::reloadScene = true;
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
		SceneAndViewSettings::GlobalScreenType = SceneAndViewSettings::ScreenType::Flat;
		try {
			try {
				std::cout << "Trying Looking Glass Bridge calibration..." << std::endl;
				SceneAndViewSettings::calibration = BridgeCalibration().getCalibration();
				calibratedBy = "Looking Glass Bridge";
			}
			catch (const std::runtime_error& e)
			{
				std::cout << "Trying USB calibration..." << std::endl;
				SceneAndViewSettings::calibration = UsbCalibration().getCalibration();
				calibratedBy = "USB Interface";
				std::cerr << e.what() << std::endl;
			}
			SceneAndViewSettings::GlobalScreenType = SceneAndViewSettings::ScreenType::LookingGlass;
			std::cout << "Calibration success: " << std::endl << SceneAndViewSettings::calibration;
		}
		catch (const AlertException& e)
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

	const std::deque<SDL_Event> emptyQueue;
	void render() override
	{
		// When rendering not event-driven
		renderOnEvent(emptyQueue);
	}
};