#pragma once
#include <filesystem>
#include <assimp/vector3.h>
#include "../FirstPersonController.h"
#include "../Calibration/Calibration.h"

inline bool wasOverridePowerSave;

namespace SceneAndViewSettings {
	inline Calibration calibration;
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

	// Read by render job and executed once. Then set to false
	inline bool recompileFShaders = false;
	inline bool reloadScene = false;
	inline struct {
		std::filesystem::path path = "";
		aiVector3D scale = { 10,10,10 };
		aiVector3D position;
		aiVector3D rotationDeg;
	} scene;
	inline int objectCountLimit = 10;
	inline GLenum debugOutput = GL_DEBUG_SEVERITY_LOW;
	inline bool pathTracing = false;
	inline std::size_t rayIteration = 0;
	inline std::size_t maxIterations = 30;
	inline std::size_t maxBounces = 3;
	inline bool interactive = false;
	inline float lightMultiplier = 5.f;
	inline float rayOffset = 1e-5f;
	inline bool subpixelOnePass = false;
	inline bool fpsWindow = false;
	inline bool backfaceCulling = true;
	inline bool skyLight = false;
	inline bool visualizeBVH;
	inline unsigned int bvhSAHthreshold = 1000000;
	inline unsigned int bvhDebugIterationsMask = 0x3;
	inline float bvhEdgeWidth = 0.3f;

	// Turn off event-driven rendering so all windows get updated every frame
	inline bool overridePowerSave = false;

	inline void stopPathTracing()
	{
		if (SceneAndViewSettings::pathTracing)
		{
			overridePowerSave = wasOverridePowerSave;
			SceneAndViewSettings::pathTracing = false;
		}
	}

	inline void startPathTracing()
	{
		if (!SceneAndViewSettings::pathTracing)
		{
			wasOverridePowerSave = overridePowerSave;
			overridePowerSave = true;
			SceneAndViewSettings::pathTracing = true;
		}
	}
};