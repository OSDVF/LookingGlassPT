#pragma once
#include "Calibration.h"

class UsbCalibration : CalibrationQuery {
public:
	Calibration getCalibration(HoloDevice device);
	Calibration getCalibration() {
		return CalibrationQuery::getCalibration();
	}
	std::vector<HoloDevice> getDevices();

	static constexpr int BUFFER_SIZE{ 255 };
	static constexpr int INTERRUPT_SIZE{ 67 };
	static constexpr int PACKET_NUM{ 7 };
	static constexpr int VENDOR_ID{ 0x04d8 };
	static constexpr int MIN_INTERFACE_NUM{ 0 };
	static constexpr int MAX_INTERFACE_NUM{ 3 };
};