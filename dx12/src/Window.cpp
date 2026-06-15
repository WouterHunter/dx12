#include "DX12PCH.h"
#include "Window.h"
#include "Context.h"
#include "ResourceStateTracker.h"

namespace {
	bool CheckTearingSupport() {
		BOOL allowTearing = false;
		ComPtr<IDXGIFactory4> factory4;
		if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)))) {
			ComPtr<IDXGIFactory5> factory5;
			if (SUCCEEDED(factory4.As(&factory5))) {
				if (FAILED(factory5->CheckFeatureSupport(
					DXGI_FEATURE_PRESENT_ALLOW_TEARING,
					&allowTearing, sizeof(allowTearing)))) {
					allowTearing = false;
				}
			}
		}
		return allowTearing;
	}
}

SwapChain::SwapChain(Context* context, HWND hWnd, ivec2 size, bool vSync, bool fullscreen)
	: context(context)
	, vSync(vSync)
	, fullscreen(fullscreen)
{
	ComPtr<IDXGISwapChain1> swapChain1;
	ComPtr<IDXGIFactory4> factory;

	allowTearing = CheckTearingSupport();

	ThrowIfFailed(CreateDXGIFactory2(CREATE_FACTORY_FLAGS, IID_PPV_ARGS(&factory)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = (UINT)size.x,
		.Height = (UINT)size.y,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = FALSE,
		.SampleDesc = { 1, 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = SWAP_CHAIN_BUFFER_COUNT,
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = (allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
			DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
	};

	const CommandQueue& commandQueue = context->CommandQueueDirect();

	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.d3dCommandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	ThrowIfFailed(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));
	ThrowIfFailed(dxgiSwapChain4->SetMaximumFrameLatency(SWAP_CHAIN_BUFFER_COUNT - 1));

	frameLatencyWaitHandle = dxgiSwapChain4->GetFrameLatencyWaitableObject();
	depthStencil = DepthStencil::CreateDefault(context, size);

	UpdateBackBuffers();
}


void SwapChain::Present() {
	UINT syncInterval = vSync ? 1 : 0;
	UINT presentFlags = allowTearing && !fullscreen && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(dxgiSwapChain4->Present(syncInterval, presentFlags));

	CommandQueue& commandQueue = context->CommandQueueDirect();
	frameFenceValues[currentBackBufferIndex] = commandQueue.Signal();
	currentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();
	renderTarget.AttachTexture(RenderTarget::Color0, backBuffers[currentBackBufferIndex]);

	commandQueue.WaitForFenceValue(frameFenceValues[currentBackBufferIndex]);
}

void SwapChain::Wait() {
	// Wait for 1 second (should never have to wait that long...)
	if (WAIT_FAILED == WaitForSingleObjectEx(frameLatencyWaitHandle, 1000, true)) {
		assert(false && "Exceeded max wait time for frame");
	}
}

void SwapChain::Resize(ivec2 size) {

	for (Ref<Texture>& backBuffer : backBuffers) {
		// Unregister from layout tracker before releasing
		if (backBuffer && backBuffer->d3d12Resource) {
			context->GetGlobalLayoutTracker().Unregister(backBuffer->d3d12Resource.Get());
		}
		// Any references to the back buffers must be released
		// before the swap chain can be resized.
		if (backBuffer) backBuffer->d3d12Resource.Reset();
	}
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(dxgiSwapChain4->GetDesc(&swapChainDesc));
	ThrowIfFailed(dxgiSwapChain4->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT,
		(UINT)size.x, (UINT)size.y,
		swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

	UpdateBackBuffers();

	depthStencil.Resize(size);
}

void SwapChain::UpdateBackBuffers() {
	for (u32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i) {
		ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
		std::wstring name = std::format(L"Back Buffer[{}]", i);
		backBuffers[i] = MakeRef<Texture>(context, backBuffer, nullptr, name.c_str());

		// Register back buffers with the global layout tracker.
		context->GetGlobalLayoutTracker().Register(backBuffer.Get(), D3D12_BARRIER_LAYOUT_COMMON);
	}
	currentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();
	renderTarget.AttachTexture(RenderTarget::Color0, backBuffers[currentBackBufferIndex]);
}

D3D12_RT_FORMAT_ARRAY SwapChain::GetRenderTargetFormats() const {
	const Ref<Resource>& backBuffer = backBuffers[currentBackBufferIndex];
	D3D12_RT_FORMAT_ARRAY rtFormats = backBuffer->GetRenderTargetFormats();
	return rtFormats;
}

Ref<Texture> SwapChain::GetCurrentBackBuffer() {
	return backBuffers[currentBackBufferIndex];
}

Window* Window::Create(Context* context, const wchar_t* title, const CommandLineArgs& args) {
	Window* window = new Window;
	window->size = { args.width, args.height };
	window->title = title;
	window->showFPS = args.showFPS;
	window->context = context;
	window->rect = { 0, 0, (LONG)args.width, (LONG)args.height };
	AdjustWindowRect(&window->rect, WS_OVERLAPPEDWINDOW, FALSE);

	window->hWnd = CreateWindowExW(
		NULL, WINDOW_CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		window->rect.right - window->rect.left,
		window->rect.bottom - window->rect.top,
		NULL, NULL, context->GetInstanceHandle(), nullptr);

	if (!window->hWnd) {
		MessageBoxA(NULL, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
		return {};
	}

	// Set pointer to this WinApp, to allow it to be retrieved in WndProc.
	SetWindowLongPtrW(window->hWnd, GWLP_USERDATA, (LONG_PTR)window);

	window->swapChain = SwapChain(context, window->hWnd, window->size, args.vSync, false);
	window->initialized = true;

	return window;
}

void Window::Destroy(Window* window) {
	delete window;
}

void Window::Resize(RECT newRect) {
	rect = newRect;
	ivec2 newSize = {
		rect.right - rect.left,
		rect.bottom - rect.top
	};

	if (newSize.x != size.x || newSize.y != size.y)
	{
		// Don't allow 0 size swap chain back buffers.
		size.x = std::max(1, newSize.x);
		size.y = std::max(1, newSize.y);

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		context->FlushAllCommandQueues();

		// Resize swap chain
		swapChain.Resize(size);
	}
}

void Window::SetShowWindow(bool show) {
	ShowWindow(hWnd, show ? SW_SHOW : SW_HIDE);
}

bool Window::PollEvents() {

	// Pump messages
	MSG msg;
	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT) {
			return false;
		}
	}

	// Store time in seconds
	timer.tick();
	deltaTime = timer.elapsedSeconds();
	totalTime = timer.totalSeconds();

	// Update rolling average delta time to find average FPS
	constexpr u64 n = 32;
	u64 tickCount = timer.tickCount();
	f64 alpha = 1.0 / (tickCount < n ? tickCount : n - 1);
	rollingAvgDelta = glm::mix(rollingAvgDelta, deltaTime, alpha);
	f64 fps = glm::round(1.0 / rollingAvgDelta);

	if (showFPS) {
		// Display the FPS in the window title bar
		std::wstring windowText = std::format(L"FPS: {:3.0f}", fps);
		SetWindowTextW(hWnd, windowText.c_str());
	}

	return true;
}

LRESULT Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Window* window = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (window && window->initialized) {
		switch (message) {
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			switch (wParam)
			{
			case 'V':
				window->swapChain.vSync = !window->swapChain.vSync;
				break;
			case VK_ESCAPE:
				::PostQuitMessage(0);
				break;
			case VK_RETURN:
				if (alt)
				{
					//SetFullscreen(!g_Fullscreen);
				}
			case VK_F11:
				break;
			}
		}
	    break;

		case WM_SIZE:
		{
			RECT clientRect = {};
			GetClientRect(hWnd, &clientRect);
			window->Resize(clientRect);
		}
		break;

		case WM_SYSCHAR:
			break;

		case WM_DESTROY:
			::PostQuitMessage(0);
			break;

		default:
			return ::DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	else {
		return ::DefWindowProcW(hWnd, message, wParam, lParam);
	}

	return 0;
}
