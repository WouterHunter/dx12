#pragma once

struct CommandLineArgs {
    i32 width{ 1280 };
	i32 height{ 720 };
	bool vSync{ false };
	bool showFPS{ false };
};

inline CommandLineArgs ParseCommandLineArguments(LPWSTR cmdLine) {
	CommandLineArgs args = {};
	i32 argc;
	wchar_t** argv = CommandLineToArgvW(cmdLine, &argc);

	for (i32 i = 0; i < argc; ++i) {
		if (::wcscmp(argv[i], L"-w") == 0)
			args.width = ::wcstol(argv[++i], nullptr, 10);
		if (::wcscmp(argv[i], L"-h") == 0)
			args.height = ::wcstol(argv[++i], nullptr, 10);
		if (::wcscmp(argv[i], L"-vsync") == 0)
			args.vSync = true;
		if (::wcscmp(argv[i], L"-fps") == 0)
			args.showFPS = true;
	}

	LocalFree((HLOCAL)argv); // Free memory allocated by CommandLineToArgvW
	return args;
}

inline void ThrowIfFailed(HRESULT result) {
	if (FAILED(result)) {
		throw std::exception();
	}
}

// Report all live objects after context destruction.
inline void ReportLiveObjects() {
#ifdef _DEBUG
	IDXGIDebug1* debugInterface;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugInterface));
	debugInterface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	debugInterface->Release();
#endif
}

struct NonCopyable {
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};


bool IsUAVCompatibleFormat(DXGI_FORMAT format);
bool IsSRGBFormat(DXGI_FORMAT format);
bool IsBGRFormat(DXGI_FORMAT format);
bool IsDepthFormat(DXGI_FORMAT format);

DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format);
DXGI_FORMAT GetUAVCompatibleFormat(DXGI_FORMAT format);
DXGI_FORMAT GetSRVFromDepthFormat(DXGI_FORMAT depthFormat);


template<class T>
constexpr DXGI_FORMAT GetFormatFromType() {
	if constexpr (std::is_same_v<T, u8>)  return DXGI_FORMAT_R8_UINT;
	if constexpr (std::is_same_v<T, i8>)  return DXGI_FORMAT_R8_SINT;
	if constexpr (std::is_same_v<T, u16>) return DXGI_FORMAT_R16_UINT;
	if constexpr (std::is_same_v<T, i16>) return DXGI_FORMAT_R16_SINT;
	if constexpr (std::is_same_v<T, u32>) return DXGI_FORMAT_R32_UINT;
	if constexpr (std::is_same_v<T, i32>) return DXGI_FORMAT_R32_SINT;
	return DXGI_FORMAT_UNKNOWN;
}

u32 GetFormatByteSize(DXGI_FORMAT format);

std::string ToString(D3D12_RESOURCE_STATES state);
std::string ToString(DXGI_FORMAT format);


// Microsoft MiniEngine Math Utility Functions
// Taken from: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Math/Common.h

template <typename T> __forceinline T AlignUpWithMask(T value, size_t mask) {
	return static_cast<T>(static_cast<size_t>(value) + mask & ~mask);
}

template <typename T> __forceinline T AlignDownWithMask(T value, size_t mask) {
	return static_cast<T>(static_cast<size_t>(value) & ~mask);
}

template <typename T> __forceinline T AlignUp(T value, size_t alignment) {
	return AlignUpWithMask(value, alignment - 1);
}

template <typename T> __forceinline T AlignDown(T value, size_t alignment) {
	return AlignDownWithMask(value, alignment - 1);
}

template <typename T> __forceinline bool IsAligned(T value, size_t alignment) {
	return 0 == (static_cast<size_t>(value) & (alignment - 1));
}

template <typename T> __forceinline T DivideByMultiple(T value, size_t alignment) {
	return static_cast<T>((value + alignment - 1) / alignment);
}

template <typename T> __forceinline bool IsPowerOfTwo(T value) {
	return 0 == (value & (value - 1));
}

template <typename T> __forceinline bool IsDivisible(T value, T divisor) {
	return (value / divisor) * divisor == value;
}