#include "DX12PCH.h"
#include "Window.h"
#include "Context.h"


DescriptorHeap CreateDescriptorHeap(const Device& device, const D3D12_DESCRIPTOR_HEAP_DESC& desc) {
	DescriptorHeap heap = {};
	ThrowIfFailed(device.d3d12Device2->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.d3dDescriptorHeap)));
	return heap;
}

DescriptorHeap CreateDescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors) {
	DescriptorHeap heap = {};
	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = type,
		.NumDescriptors = numDescriptors,
	};
	ThrowIfFailed(device.d3d12Device2->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.d3dDescriptorHeap)));
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
	u32 rtvDescriptorSize = device.d3d12Device2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvVDescriptorHeap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (u32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i) {
		Resource& backBuffer = backBuffers[i];
		backBuffer.cpuHandle = rtvHandle;
		ThrowIfFailed(dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&backBuffer.d3d12Resource)));
		ThrowIfFailed(backBuffer.d3d12Resource->SetName((L"Back Buffer[" + std::to_wstring(i) + L"]").c_str()));
		device.d3d12Device2->CreateRenderTargetView(backBuffer.d3d12Resource.Get(), nullptr, rtvHandle);
		rtvHandle.Offset((INT)rtvDescriptorSize);
	}
}

Resource* SwapChain::GetBackBuffer() {
	Resource* backBuffer = backBuffers + currentBackBufferIndex;
	return backBuffer;
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

		for (Resource& backBuffer : swapChain.backBuffers) {
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			backBuffer.d3d12Resource.Reset();
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

	// Get delta time and total time in seconds
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	f64 delta = f64(currentTime.QuadPart - lastTime);
	f64 total = f64(currentTime.QuadPart - startTime);
	lastTime = currentTime.QuadPart;

	// Store time in seconds
	deltaTime = delta * invPerfFreq;
	totalTime = total * invPerfFreq;

	// Update the window title with current FPS every second
	if (totalTime - floor(totalTime) < deltaTime) {
		constexpr size_t BUFFER_SIZE = 256;
		WCHAR buffer[BUFFER_SIZE];
		f64 fps = round(1.0 / deltaTime);
		f64 mspf = 1000.0 / fps;
		swprintf_s(buffer, BUFFER_SIZE, L"FPS: %3.0f | MS/Frame: %2.1f", fps, mspf);
		SetWindowTextW(hWnd, buffer);
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
