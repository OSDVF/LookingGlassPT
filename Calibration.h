#pragma once
#include <vector>
#include <nlohmann/json.hpp>
#include <ostream>

struct Calibration {
	// Based on https://github.com/dormon/3DApps/blob/master/src/holoCalibration.h
	float pitch = 47.6004f;
	float slope = -5.48489;
	float center = -0.400272;
	float viewCone;
	float invView;
	float verticalAngle;
	float DPI = 338;
	float screenW = 2560;
	float screenH = 1600;
	float flipImageX;
	float flipImageY;
	float flipSubp;

	float recalculatedPitch() const { return pitch * (screenW / DPI) * cos(atan(1.0 / slope)); }
	float tilt() const { return (screenH / (screenW * slope)) * ((flipImageX == 1.0) ? -1 : 1); }
	float subp() const { return 1.0 / (screenW * 3.0); }

	struct { float pitch; float tilt; float center; } forShader()
	{
		return {
			recalculatedPitch(), tilt(), center
		};
	}

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
	Calibration getCalibration() {
		return getCalibration(getDevices().front());
	}
	virtual std::vector<HoloDevice> getDevices() = 0;

	Calibration jsonToCaibration(nlohmann::json j)
	{
		return Calibration{
			static_cast<float>(j["pitch"]["value"]),
			static_cast<float>(j["slope"]["value"]),
			static_cast<float>(j["center"]["value"]),
			static_cast<float>(j["viewCone"]["value"]),
			static_cast<float>(j["invView"]["value"]),
			static_cast<float>(j["verticalAngle"]["value"]),
			static_cast<float>(j["DPI"]["value"]),
			static_cast<float>(j["screenW"]["value"]),
			static_cast<float>(j["screenH"]["value"]),
			static_cast<float>(j["flipImageX"]["value"]),
			static_cast<float>(j["flipImageY"]["value"]),
			static_cast<float>(j["flipSubp"]["value"]),
		};
	}
};