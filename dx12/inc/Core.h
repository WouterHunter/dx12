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
#include <dxgidebug.h> 
#include <d3dcompiler.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include "directx/DirectXTex.h"

// STL
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

namespace fs = std::filesystem;
namespace pmr = std::pmr;
using std::chrono::nanoseconds;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;

template<class T> using Ptr = std::unique_ptr<T>;
template<class T> using Ref = std::shared_ptr<T>;

template<class T, class ... Args>
constexpr Ptr<T> MakePtr(Args&& ... args) {
	return std::make_unique<T>(std::forward<Args>(args)...);
}
template<class T, class ... Args>
constexpr Ref<T> MakeRef(Args&& ... args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}


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
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_SILENT_WARNINGS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

using CPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE;
using GPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE;

constexpr u32 INVALID_INDEX = ~0u;
constexpr u64 INVALID_FENCE_VALUE = ~0ull;

#ifdef _DEBUG
constexpr u32 CREATE_FACTORY_FLAGS = DXGI_CREATE_FACTORY_DEBUG;
#else
constexpr u32 CREATE_FACTORY_FLAGS = 0;
#endif

#include "Logging.h"