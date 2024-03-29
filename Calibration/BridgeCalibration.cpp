#include "BridgeCalibration.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable, std::cv_status
#include <fmt/core.h>
#include <process.hpp>
#include "../Structures/AlertException.h"
#include "../Helpers.h"
#ifdef _WIN32
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
	const std::string allowAwaitArg16 = "--harmony-top-level-await";
	const std::string allowAwaitArg18 = "--experimental-repl-await";
	std::string alllowAwaitArgument = allowAwaitArg16;

	std::mutex mtx;
	std::unique_lock<std::mutex> lck(mtx);

	std::cout << "Checking Node.js version" << std::endl;
	bool versionChecked = false;
	Process nodeExists("node -v", "", [&](const char* bytes, size_t n) {
		if (!versionChecked)
		{
			//Extract the major version
			std::string version(bytes, n);
			if (version.starts_with("v"))
			{
				version = version.substr(1);
			}
			auto dot = version.find('.');
			if (dot != std::string::npos)
			{
				version = version.substr(0, dot);
			}
			if (std::stoi(version) > 16)
			{
				alllowAwaitArgument = allowAwaitArg18;
			}
			versionChecked = true;
		}
		}
	);
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

	Process bridgeCalibrationProcess(
		fmt::format(
			"node "
#ifdef _DEBUG
			"--unhandled-rejections=strict "
#endif
			"{} {}",
			alllowAwaitArgument,
			Helpers::relativeToExecutable("index.js").string()
		), "",
		[&](const char* bytes, size_t n)
		{
			auto stringVersion = std::string(bytes, n);
			if (stringVersion.starts_with("alert: "))
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
			auto stringVersion = std::string(bytes, n);
			std::cerr << "Bridge Error: " << stringVersion;
			if (stringVersion.starts_with("no devices connected"))
			{
				alerts << "No Looking Glass devices connected." << std::endl;
			}
			// Add a newline for prettier output on some platforms:
			if (bytes[n - 1] != '\n')
				std::cout << std::endl;
		}
	);
	int exit_status;
	std::thread thread([&]() {
		exit_status = bridgeCalibrationProcess.get_exit_status();
		cv.notify_one();
		}
	);
	if (!bridgeCalibrationProcess.try_get_exit_status(exit_status) && cv.wait_for(lck, std::chrono::seconds(TIMEOUT)) == std::cv_status::timeout)
	{
		bridgeCalibrationProcess.kill();
		thread.detach();
		throw std::runtime_error(fmt::format("Looking Glass Bridge not responding in {} seconds", TIMEOUT));
	}
	thread.join();
	if (alerts.tellp() > 0)
	{
		throw AlertException(alerts.str());
	}
	if (exit_status != 0)
	{
		throw std::runtime_error(fmt::format("Node.js exited with code {}", exit_status));
	}
	return result;
}
std::vector<HoloDevice> BridgeCalibration::getDevices()
{
	return { HoloDevice{-1} };
}