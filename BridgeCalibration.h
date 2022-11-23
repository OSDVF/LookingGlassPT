#pragma once
#include "Calibration.h"

class BridgeCalibration : CalibrationQuery {
public:
	Calibration getCalibration(HoloDevice device);
	Calibration getCalibration() {
		return CalibrationQuery::getCalibration();
	}
	std::vector<HoloDevice> getDevices();

#ifdef _DEBUG
	static inline int TIMEOUT = 5;
#else
	static inline int TIMEOUT = 10;
#endif
};