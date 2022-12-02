#pragma once
#include "FirstPersonController.h"
namespace ProjectSettings {
	inline float fov = 60;
	inline float mouseX = 100;
	inline float mouseY = 100;
	inline float farPlane = 1000;
	inline float nearPlane = 0.1;
	inline float viewCone = 40;
	inline float focusDistance = 2.f;
	inline FirstPersonController person;
	inline enum class ScreenType {
		Flat = 0, LookingGlass = 1
	} GlobalScreenType;
	inline bool changeScreenType = false;
	inline bool recompileFShaders = false;
};