#pragma once
#include "Resource.h"
#include "Timer.h"

constexpr u32 SWAP_CHAIN_BUFFER_COUNT = 3;

struct Device;
struct Context;


/** Descriptor heap */
struct DescriptorHeap {
	ComPtr<ID3D12DescriptorHeap> d3dDescriptorHeap;
};

DescriptorHeap CreateDescriptorHeap(const Device& device, const D3D12_DESCRIPTOR_HEAP_DESC& desc);
DescriptorHeap CreateDescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors);


/** Swap chain */
struct SwapChain {

	void Present();
	void Wait();
	void UpdateBackBuffers();
	D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;
	Ref<Resource> GetCurrentBackBuffer();

	Context* context;
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	HANDLE frameLatencyWaitHandle;

	DescriptorHeap rtvVDescriptorHeap;
	u32 rtvDescriptorSize = 0;
	u32 currentBackBufferIndex = 0;
	u64 frameFenceValues[SWAP_CHAIN_BUFFER_COUNT] = {};
	Ref<Resource> backBuffers[SWAP_CHAIN_BUFFER_COUNT];

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

	ivec2 size;
	std::wstring title;

	b8 initialized = false;
	b8 showFPS = false;

	Context* context;
	SwapChain swapChain;

	Timer timer;
	f64 deltaTime;
	f64 totalTime;
	f64 rollingAvgDelta;
};