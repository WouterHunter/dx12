#pragma once

// STL
#include "Device.h"
#include "CommandQueue.h"
#include "Window.h"

constexpr u32 SWAP_CHAIN_BUFFER_COUNT = 3;


/** Descriptor heap */
struct DescriptorHeap {
	ComPtr<ID3D12DescriptorHeap> d3dDescriptorHeap;
};

DescriptorHeap CreateDescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors);


/** Resource */
struct Resource {
	ComPtr<ID3D12Resource> resource;
};


/** Swap chain */
struct SwapChain {

	void Present();
	void Wait();
	void UpdateBackBuffers();

	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	HANDLE frameLatencyWaitHandle;

	DescriptorHeap rtvVDescriptorHeap;
	u32 rtvDescriptorSize;
	u32 currentBackBufferIndex;
	u64 frameFenceValues[SWAP_CHAIN_BUFFER_COUNT];
	Resource backBuffers[SWAP_CHAIN_BUFFER_COUNT];

	b8 vSync = true;
	b8 allowTearing = false;
	b8 fullscreen = false;
};


/** D3D12 library context singleton */
struct Context {
	static void Create(HINSTANCE hInst);
	static void Destroy();

	static struct Window CreateWindow(const wchar_t* title, ivec2 size, bool vSync = true);
	static void DestroyWindow(Window& window);

	static void FlushAllCommandQueues();
	static void Quit(int exitCode = 0);

	static Device& GetDevice();
	static CommandQueue& CommandQueueDirect();
	static CommandQueue& CommandQueueCompute();
	static CommandQueue& CommandQueueCopy();

	// copy + move + assign ctors -> delete

private:
	Context() = default;

	HINSTANCE hInstance;
	Adapter adapter;
	Device device;

	CommandQueue commandQueueDirect{};
	CommandQueue commandQueueCompute{};
	CommandQueue commandQueueCopy{};
};


/** Win32/D3D12 swap-chain wrapper */
struct Window {

	HWND hWnd; // Window handle.
	RECT rect; // Window rectangle (used to toggle fullscreen state).

	ivec2 size = { 1280, 720 };
	b8 initialized = false;

	Context* context;
	SwapChain swapChain;

	u64 frameCounter;
	u64 lastTime;
	u64 startTime;
	f64 invPerfFreq;
	f64 deltaTime;
	f64 totalTime;
};

void SetWindowPointer(Window& window);
void CreateWindowSwapChain(Window& window);
void ResizeWindow(Window& window, ivec2 size);
void SetShowWindow(Window& window, bool show);
bool WindowPollEvents(Window& window);
