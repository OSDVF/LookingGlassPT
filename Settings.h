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
	inline bool reloadScene = true;
	inline struct {
		std::filesystem::path path = "cornellBox.glb";
		aiVector3D scale = { 10,10,10 };
		aiVector3D position;
		aiVector3D rotationDeg;
	} scene;
	inline int objectCountLimit = 10;
	inline GLenum debugOutput = GL_DEBUG_SEVERITY_LOW;
	inline bool pathTracing = false;
	inline std::size_t rayIteration = 0;
	inline bool interactive = false;
	inline float lightMultiplier = 5.f;
};