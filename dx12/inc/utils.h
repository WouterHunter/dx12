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