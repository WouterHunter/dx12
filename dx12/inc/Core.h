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
#include <functional>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <queue>
#include <ranges>
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

// DirectX Aliases
using CPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE;
using GPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE;

// DirectX Default Values
constexpr f32				DEFAULT_CLEAR_COLOR[]			 = { 0.1f, 0.15f, 0.15f, 1.0f };
constexpr f32				DEFAULT_CLEAR_DEPTH				 = 1.0f;
constexpr u8				DEFAULT_CLEAR_STENCIL			 = 0;
constexpr DXGI_FORMAT		DEFAULT_BACK_BUFFER_FORMAT		 = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT		DEFAULT_DEPTH_BUFFER_FORMAT		 = DXGI_FORMAT_D32_FLOAT;
constexpr DXGI_FORMAT		DEFAULT_SRGB_FORMAT				 = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr D3D12_CLEAR_VALUE	DEFAULT_BACK_BUFFER_CLEAR_VALUE	 = { DEFAULT_BACK_BUFFER_FORMAT, { 0, 0, 0, 0 } };
constexpr D3D12_CLEAR_VALUE	DEFAULT_DEPTH_BUFFER_CLEAR_VALUE = { DEFAULT_DEPTH_BUFFER_FORMAT, { 1, 0 } };
constexpr D3D12_RECT		DEFAULT_SCISSOR_RECT			 = { 0, 0, LONG_MAX, LONG_MAX };

constexpr u32 INVALID_INDEX = ~0u;
constexpr u64 INVALID_FENCE_VALUE = ~0ull;
constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12 Render Window";

#ifdef _DEBUG
constexpr u32 CREATE_FACTORY_FLAGS = DXGI_CREATE_FACTORY_DEBUG;
#else
constexpr u32 CREATE_FACTORY_FLAGS = 0;
#endif
