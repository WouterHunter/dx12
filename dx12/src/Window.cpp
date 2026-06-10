#include "DX12PCH.h"
#include "Window.h"
#include "Context.h"
#include "ResourceStateTracker.h"


DescriptorHeap CreateDescriptorHeap(const Device& device, const D3D12_DESCRIPTOR_HEAP_DESC& desc) {
	DescriptorHeap heap = {};
	ThrowIfFailed(device.d3d12Device10->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.d3dDescriptorHeap)));
	return heap;
}

DescriptorHeap CreateDescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors) {
	DescriptorHeap heap = {};
	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = type,
		.NumDescriptors = numDescriptors,
	};
	ThrowIfFailed(device.d3d12Device10->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.d3dDescriptorHeap)));
	return heap;
}

void SwapChain::Present() {
	UINT syncInterval = vSync ? 1 : 0;
	UINT presentFlags = allowTearing && !fullscreen && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(dxgiSwapChain4->Present(syncInterval, presentFlags));

	CommandQueue& commandQueue = context->CommandQueueDirect();
	frameFenceValues[currentBackBufferIndex] = commandQueue.Signal();
	currentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();
	commandQueue.WaitForFenceValue(frameFenceValues[currentBackBufferIndex]);
}

void SwapChain::Wait() {
	// Wait for 1 second (should never have to wait that long...)
	if (WAIT_FAILED == WaitForSingleObjectEx(frameLatencyWaitHandle, 1000, true)) {
		assert(false && "Exceeded max wait time for frame");
	}
}

void SwapChain::UpdateBackBuffers() {
	const Device& device = context->GetDevice();
	u32 rtvDescriptorSize = device.d3d12Device10->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvVDescriptorHeap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (u32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i) {
		backBuffers[i] = MakeRef<Resource>();
		Ref<Resource>& backBuffer = backBuffers[i];
		backBuffer->cpuHandle = rtvHandle;
		ThrowIfFailed(dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&backBuffer->d3d12Resource)));
		ThrowIfFailed(backBuffer->d3d12Resource->SetName((L"Back Buffer[" + std::to_wstring(i) + L"]").c_str()));
		device.d3d12Device10->CreateRenderTargetView(backBuffer->d3d12Resource.Get(), nullptr, rtvHandle);
		rtvHandle.Offset((INT)rtvDescriptorSize);

		// Register back buffers with the global layout tracker (initial layout is PRESENT/COMMON)
		context->GetGlobalLayoutTracker().Register(
			backBuffer->d3d12Resource.Get(), D3D12_BARRIER_LAYOUT_PRESENT);
	}
}

D3D12_RT_FORMAT_ARRAY SwapChain::GetRenderTargetFormats() const {
	const Ref<Resource>& backBuffer = backBuffers[currentBackBufferIndex];
	D3D12_RT_FORMAT_ARRAY rtFormats = backBuffer->GetRenderTargetFormats();
	return rtFormats;
}

Ref<Resource> SwapChain::GetCurrentBackBuffer() {
	return backBuffers[currentBackBufferIndex];
}

void Window::Resize(ivec2 size) {
	if (size.x != size.x || size.y != size.y)
	{
		// Don't allow 0 size swap chain back buffers.
		this->size.x = std::max(1, size.x);
		this->size.y = std::max(1, size.y);

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		context->FlushAllCommandQueues();

		for (Ref<Resource>& backBuffer : swapChain.backBuffers) {
			// Unregister from layout tracker before releasing
			if (backBuffer && backBuffer->d3d12Resource) {
				context->GetGlobalLayoutTracker().Unregister(backBuffer->d3d12Resource.Get());
			}
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			if (backBuffer) backBuffer->d3d12Resource.Reset();
		}
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(swapChain.dxgiSwapChain4->GetDesc(&swapChainDesc));
		ThrowIfFailed(swapChain.dxgiSwapChain4->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT,
			this->size.x, this->size.y,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		swapChain.currentBackBufferIndex = swapChain.dxgiSwapChain4->GetCurrentBackBufferIndex();
		swapChain.UpdateBackBuffers();
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

		case WM_SIZE: {
			RECT clientRect = {};
			GetClientRect(hWnd, &clientRect);
			window->Resize({
				clientRect.right - clientRect.left,
				clientRect.bottom - clientRect.top
				});
		}
					break;
					// The default window procedure will play a system notification sound 
					// when pressing the Alt+Enter keyboard combination if this message is 
					// not handled.
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
