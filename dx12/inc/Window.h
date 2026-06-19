#pragma once
#include "Resource.h"
#include "RenderTarget.h"
#include "Timer.h"

constexpr u32 SWAP_CHAIN_BUFFER_COUNT = 3;

class Context;
class Device;
class Window;

/** Swap chain */
class SwapChain {
public:
	
	SwapChain() = default;
	SwapChain(Context* context, HWND hWnd, ivec2 size, bool vSync, bool fullscreen);

	void Present();
	void Wait();
	void Resize(ivec2 size);
	void UpdateBackBuffers();
	D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;
	Ref<Texture> GetCurrentBackBuffer();

	Context* context{};
	ComPtr<IDXGISwapChain4> dxgiSwapChain4{};
	HANDLE frameLatencyWaitHandle{};

	u32 currentBackBufferIndex = 0;
	u64 frameFenceValues[SWAP_CHAIN_BUFFER_COUNT] = {};
	Ref<Texture> backBuffers[SWAP_CHAIN_BUFFER_COUNT];
	RenderTarget renderTarget;
	DepthStencil depthStencil;

	b8 vSync = true;
	b8 allowTearing = false;
	b8 fullscreen = false;
};

/** Win32/D3D12 swap-chain wrapper */
class Window : NonCopyable {
public:

	static Window* Create(Context* context, const wchar_t* title, const CommandLineArgs& args);
	static void Destroy(Window * window);

	void Resize(RECT newRect);
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