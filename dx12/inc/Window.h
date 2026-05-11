#pragma once

constexpr u32 SWAP_CHAIN_BUFFER_COUNT = 3;

struct Device;
struct Context;


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

	Context* context;
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	HANDLE frameLatencyWaitHandle;

	DescriptorHeap rtvVDescriptorHeap;
	u32 rtvDescriptorSize = 0;
	u32 currentBackBufferIndex = 0;
	u64 frameFenceValues[SWAP_CHAIN_BUFFER_COUNT] = {};
	Resource backBuffers[SWAP_CHAIN_BUFFER_COUNT] = {};

	b8 vSync = true;
	b8 allowTearing = false;
	b8 fullscreen = false;
};

/** Win32/D3D12 swap-chain wrapper */
struct Window {

	void Resize(ivec2 size);
	void SetShowWindow(bool show);
	bool PollEvents();

	// Win32 message callback
	static LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

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