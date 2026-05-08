#pragma once

// Win32 includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#ifdef CreateWindow
#undef CreateWindow
#endif

#include <wrl.h>
using Microsoft::WRL::ComPtr;


// DirectX headers
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

// STL
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
using std::chrono::nanoseconds;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;


#include "Core.h"
#include "Utils.h"