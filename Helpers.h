#pragma once
#include <filesystem>
#include <string>
#include <SDL.h>
#ifdef WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
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
#else
			96.0f;
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

#ifdef _WIN32
	const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	inline void SetThreadName(uint32_t dwThreadID, const char* threadName)
	{

		// DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = threadName;
		info.dwThreadID = dwThreadID;
		info.dwFlags = 0;

		__try
		{
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
	inline void SetThreadName(const char* threadName)
	{
		SetThreadName(GetCurrentThreadId(), threadName);
	}

	inline void SetThreadName(std::thread* thread, const char* threadName)
	{
		DWORD threadId = ::GetThreadId(static_cast<HANDLE>(thread->native_handle()));
		SetThreadName(threadId, threadName);
	}

#elif defined(__linux__)
	inline void SetThreadName(const char* threadName)
	{
		prctl(PR_SET_NAME, threadName, 0, 0, 0);
	}
#elif defined(__APPLE__)
	inline void SetThreadName(const char* threadName)
	{
		pthread_setname_np(threadName);
	}

#else
	inline void SetThreadName(std::thread* thread, const char* threadName)
	{
		auto handle = thread->native_handle();
		pthread_setname_np(handle, threadName);
	}
#endif
}