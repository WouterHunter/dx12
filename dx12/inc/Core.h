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
#include <iostream>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
using std::chrono::nanoseconds;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;


using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using b8 = bool;

using f32 = float;
using f64 = double;

using size_t = uint64_t;
using ptr_t = uintptr_t;

#define GLM_FORCE_AVX2
#define GLM_FORCE_SILENT_WARNINGS
#include <glm/glm.hpp>

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat2;
using glm::mat3;
using glm::mat4;
using glm::quat;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;

constexpr u64 INVALID_FENCE_VALUE = ~0ull;

#ifdef _DEBUG
constexpr u32 CREATE_FACTORY_FLAGS = DXGI_CREATE_FACTORY_DEBUG;
#elif
constexpr u32 CREATE_FACTORY_FLAGS = 0;
#endif