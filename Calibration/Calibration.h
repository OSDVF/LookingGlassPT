#pragma once
#include "../PrecompiledHeaders.hpp"
#include <ostream>

struct Calibration {
	// Based on https://github.com/dormon/3DApps/blob/master/src/holoCalibration.h
	float pitch = 47.6004f;
	float slope = -5.48489f;
	float center = -0.400272f;
	float viewCone;
	float invView;
	float verticalAngle;
	float DPI = 338;
	float screenW = 2560;
	float screenH = 1600;
	float flipImageX;
	float flipImageY;
	float flipSubp;

	float recalculatedPitch() const;
	float tilt() const;
	float subp() const;

	struct ForShader {
		float pitch;
		float tilt;
		float center;
		float subp;//This is not really used
		glm::vec2 resolution;
	};
	ForShader forShader();

	friend std::ostream& operator<<(std::ostream& os, Calibration const& cal) {
		return os << "Pitch:\t" << cal.pitch << std::endl
			<< "Pitch_R:\t" << cal.recalculatedPitch() << std::endl
			<< "Slope:\t" << cal.slope << std::endl
			<< "Tilt:\t" << cal.tilt() << std::endl
			<< "Center:\t" << cal.center << std::endl
			<< "ViewCone:\t" << cal.viewCone << std::endl
			<< "InvView:\t" << cal.invView << std::endl
			<< "VerticalAngle:\t" << cal.verticalAngle << std::endl
			<< "DPI:\t" << cal.DPI << std::endl
			<< "ScreenWidth:\t" << cal.screenW << std::endl
			<< "ScreenHeight:\t" << cal.screenH << std::endl
			<< "FlipImageX:\t" << cal.flipImageX << std::endl
			<< "FlipImageY:\t" << cal.flipImageY << std::endl
			<< "FlipSubp:\t" << cal.flipSubp << std::endl
			<< "Subp:\t" << cal.subp() << std::endl
			<< std::endl;
	};
};

struct HoloDevice {
	int productId;
};

class CalibrationQuery {
public:
	virtual Calibration getCalibration(HoloDevice device) = 0;
	Calibration getCalibration();
	virtual std::vector<HoloDevice> getDevices() = 0;

	Calibration jsonToCaibration(nlohmann::json j);
};