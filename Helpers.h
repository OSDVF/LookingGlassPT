#pragma once
#include <filesystem>
#include <string>
#include <SDL.h>
#ifdef WIN32
#include <Windows.h>
#endif

namespace Helpers
{
	/**
	* Resolves to a file in current working directory. But if it does not exists, resolves to file next to the executable
	*/
	inline std::filesystem::path relativeToExecutable(std::string filename)
	{
		if (std::filesystem::exists(filename))
		{
			return filename;
		}

		std::filesystem::path executablePath;
#ifdef WIN32
		LPSTR exePath = new CHAR[MAX_PATH];
		GetModuleFileNameA(NULL, exePath, MAX_PATH);
		executablePath = std::filesystem::path(exePath);
		delete[] exePath;
#else
		executablePath = std::filesystem::canonical("/proc/self/exe");
#endif

		return executablePath.parent_path() / filename;
	}

	inline void GetDisplayDPI(int displayIndex, float* dpi, float* defaultDpi)
	{
		const float kSysDefaultDpi =
#ifdef __APPLE__
			72.0f;
#elif defined(_WIN32)
			96.0f;
#else
			static_assert(false, "No system default DPI set for this platform.");
#endif

		if (SDL_GetDisplayDPI(displayIndex, NULL, dpi, NULL) != 0)
		{
			// Failed to get DPI, so just return the default value.
			if (dpi) *dpi = kSysDefaultDpi;
		}

		if (defaultDpi) *defaultDpi = kSysDefaultDpi;
	}

	inline float GetVirtualPixelScale(SDL_Window* w)
	{
		float dpi, defaultDpi;
		GetDisplayDPI(SDL_GetWindowDisplayIndex(w), &dpi, &defaultDpi);
		return dpi / defaultDpi;
	}
}