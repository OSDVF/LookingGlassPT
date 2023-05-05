#include "Calibration.h"

float Calibration::recalculatedPitch() const {
    return pitch * (screenW / DPI) * cos(atan(1.0f / slope));
}
float Calibration::tilt() const { return (screenH / (screenW * slope)) * ((flipImageX == 1.0f) ? -1 : 1); }
float Calibration::subp() const { return 1.0f / (screenW * 3.0f); }

Calibration::ForShader Calibration::forShader()
{
    return {
        recalculatedPitch(), tilt(), center, subp(), {screenW, screenH}
    };
}

Calibration CalibrationQuery::getCalibration()
{
    return getCalibration(getDevices().front());
}

Calibration CalibrationQuery::jsonToCaibration(nlohmann::json j)
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