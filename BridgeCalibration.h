#pragma once
#include "Calibration.h"

class BridgeCalibration : CalibrationQuery {
public:
	Calibration getCalibration(HoloDevice device);
	Calibration getCalibration() {
		return CalibrationQuery::getCalibration();
	}
	std::vector<HoloDevice> getDevices();

	static inline int TIMEOUT = 10;
};