#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <exception>

inline void ThrowIfFailed(HRESULT result) {
	if (FAILED(result)) {
		throw std::exception();
	}
}

inline void SetThreadName(std::thread& thread, const std::wstring& name) {
	HANDLE nativeHandle = thread.native_handle();
	ThrowIfFailed(SetThreadDescription(nativeHandle, name.c_str()));
}

inline std::string GetWin32ErrorMessage()
{
	DWORD errorCode = GetLastError();

	if (errorCode == 0)
	{
		return "";
	}

	LPWSTR wideBuffer = nullptr;

	DWORD size = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&wideBuffer,
		0,
		nullptr);

	if (size == 0 || !wideBuffer)
	{
		return "Unknown Win32 error";
	}

	int utf8Size = WideCharToMultiByte(
		CP_UTF8,
		0,
		wideBuffer,
		-1,
		nullptr,
		0,
		nullptr,
		nullptr);

	std::string result;
	result.resize(utf8Size - 1);

	WideCharToMultiByte(
		CP_UTF8,
		0,
		wideBuffer,
		-1,
		result.data(),
		utf8Size,
		nullptr,
		nullptr);

	LocalFree(wideBuffer);

	return result;
}