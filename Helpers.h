#pragma once
#include <filesystem>
#include <string>
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
}