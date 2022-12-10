#pragma once
#include <filesystem>
#include <assimp/vector3.h>
#include "FirstPersonController.h"
namespace ProjectSettings {
	inline float fov = 60;
	inline float mouseX = 100;
	inline float mouseY = 100;
	inline float farPlane = 1000;
	inline float nearPlane = 0.1f;
	inline float viewCone = 40;
	inline float focusDistance = 2.f;
	inline FirstPersonController person;
	inline enum class ScreenType {
		Flat = 0, LookingGlass = 1
	} GlobalScreenType;
	inline bool applyScreenType = false;
	inline bool recompileFShaders = false;
	inline bool reloadScene = false;
	inline struct {
		std::filesystem::path path = "";
		aiVector3D scale = { 1,1,1 };
		aiVector3D position;
		aiVector3D rotationDeg ;
	} scene;
	inline int objectCountLimit = 2;
	inline GLenum debugOutput = GL_DEBUG_SEVERITY_LOW;
};