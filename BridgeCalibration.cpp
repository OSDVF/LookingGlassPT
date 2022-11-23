#include "BridgeCalibration.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable, std::cv_status
#include <format>
#include <process.hpp>
#include <nlohmann/json.hpp>
#include "alert_exception.h"
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

using namespace TinyProcessLib;
using namespace std::chrono_literals;

const std::array calibrationProperties = {
	"pitch",
	"slope",
	"center",
	"viewCone",
	"invView",
	"verticalAngle",
	"DPI",
	"screenW",
	"screenH",
	"flipImageX",
	"flipImageY",
	"flipSubp"
};

std::condition_variable cv;
Calibration result;
std::stringstream alerts;
Calibration BridgeCalibration::getCalibration(HoloDevice device)
{
	std::mutex mtx;
	std::unique_lock<std::mutex> lck(mtx);

	std::cout << "Checking Node.js version" << std::endl;
	Process nodeExists("node -v");
	if (nodeExists.get_exit_status() != 0)
	{
		char choice = 'y';
		if (isatty(fileno(stdout))) {
			//We are running inside a console
			std::cout << "Node.js existence check failed. Try to run it anyway? [Y/N]" << std::endl;
			std::cin >> choice;
		}
		// Else just let everything run
		if (std::tolower(choice) == 'n')
		{
			throw std::runtime_error("Node.js skipped.");
		}
	}

	Process process(
		"node "
#ifdef _DEBUG
		"--unhandled-rejections=strict "
#endif
		"--harmony-top-level-await index.js", "",
		[&](const char* bytes, size_t n)
		{
			auto stringVersion = std::string(bytes, n);
	if (stringVersion.substr(0, 7) == "alert: ")
	{
		alerts << stringVersion.substr(7) << std::endl;
	}
	else
	{
		std::cout << "Looking Glass Bridge: " << stringVersion;
	}
	try
	{
		auto parsed = nlohmann::json::parse(stringVersion);
		bool correctFormat = true;
		for (int i = 0; i < calibrationProperties.size(); i++)
		{
			if (!parsed.contains(calibrationProperties[i]))
			{
				correctFormat = false;
				std::cerr << "Ignored this JSON because it does not contain " << calibrationProperties[i] << std::endl;
				break;
			}
		}
		if (correctFormat)
		{
			// Finally success
			result = jsonToCaibration(parsed);
			cv.notify_one();
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		std::cout << "Could not parse that into JSON" << std::endl;
	}
		},
		[&](const char* bytes, size_t n) {
			std::cerr << "Bridge Error: " << std::string(bytes, n);
		// Add a newline for prettier output on some platforms:
		if (bytes[n - 1] != '\n')
			std::cout << std::endl;
		}
		);
	int exit_status;
	std::thread thread([&]() {process.get_exit_status(); });
	if (cv.wait_for(lck, std::chrono::seconds(TIMEOUT)) == std::cv_status::timeout && !process.try_get_exit_status(exit_status))
	{
		process.kill();
		thread.detach();
		throw std::runtime_error(std::format("Looking Glass Bridge not responding in {} seconds", TIMEOUT));
	}
	thread.join();
	if (alerts.tellp() > 0)
	{
		throw alert_exception(std::format("Looking Glass Bridge says: {}", alerts.str()));
	}
	return result;
}
std::vector<HoloDevice> BridgeCalibration::getDevices()
{
	return { HoloDevice{-1} };
}