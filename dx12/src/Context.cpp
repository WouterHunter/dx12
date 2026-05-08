#include "PCH.h"
#include "Context.h"
#include "resource.h" // icon resource

#include <shlwapi.h>
#include <fcntl.h> 
#include <corecrt_io.h>

#include <iostream>
#include <mutex>

#include "Utils.h"

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12 Render Window";

// Context singleton instance
static Context* context = nullptr;

// Win32 message callback
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

namespace {
	/**
	 * Create a console window (consoles are not automatically created for Windows
	 * subsystems)
	 */
	void CreateConsole()
	{
		// Allocate a console.
		if (AllocConsole())
		{
			HANDLE lStdHandle = GetStdHandle(STD_OUTPUT_HANDLE);

			// Increase screen buffer to allow more lines of text than the default.
			CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
			GetConsoleScreenBufferInfo(lStdHandle, &consoleInfo);
			consoleInfo.dwSize.Y = 1024;
			SetConsoleScreenBufferSize(lStdHandle, consoleInfo.dwSize);
			SetConsoleCursorPosition(lStdHandle, { 0, 0 });

			// Redirect unbuffered STDOUT to the console.
			int   hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
			FILE* fp = _fdopen(hConHandle, "w");
			freopen_s(&fp, "CONOUT$", "w", stdout);
			setvbuf(stdout, nullptr, _IONBF, 0);

			// Redirect unbuffered STDIN to the console.
			lStdHandle = GetStdHandle(STD_INPUT_HANDLE);
			hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
			fp = _fdopen(hConHandle, "r");
			freopen_s(&fp, "CONIN$", "r", stdin);
			setvbuf(stdin, nullptr, _IONBF, 0);

			// Redirect unbuffered STDERR to the console.
			lStdHandle = GetStdHandle(STD_ERROR_HANDLE);
			hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
			fp = _fdopen(hConHandle, "w");
			freopen_s(&fp, "CONOUT$", "w", stderr);
			setvbuf(stderr, nullptr, _IONBF, 0);

			// Clear the error state for each of the C++ standard stream objects. We
			// need to do this, as attempts to access the standard streams before
			// they refer to a valid target will cause the iostream objects to enter
			// an error state. In versions of Visual Studio after 2005, this seems
			// to always occur during startup regardless of whether anything has
			// been read from or written to the console or not.
			std::wcout.clear();
			std::cout.clear();
			std::wcerr.clear();
			std::cerr.clear();
			std::wcin.clear();
			std::cin.clear();
		}
	}

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

DescriptorHeap CreateDescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors) {
	DescriptorHeap heap = {};
	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = type,
		.NumDescriptors = numDescriptors,
	};
	ThrowIfFailed(device.dxgiDevice2->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.d3dDescriptorHeap)));

	return heap;
}

void CreateWindowSwapChain(Window& window) {
	SwapChain& swapChain = window.swapChain;
	ComPtr<IDXGISwapChain1> swapChain1;
	ComPtr<IDXGIFactory4> factory;

	ThrowIfFailed(CreateDXGIFactory2(CREATE_FACTORY_FLAGS, IID_PPV_ARGS(&factory)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = (UINT)window.size.x,
		.Height = (UINT)window.size.y,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = FALSE,
		.SampleDesc = { 1, 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = SWAP_CHAIN_BUFFER_COUNT,
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = (CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
			DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
	};

	const CommandQueue& commandQueue = Context::CommandQueueDirect();

	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.d3dCommandQueue.Get(),
		window.hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	ThrowIfFailed(factory->MakeWindowAssociation(window.hWnd, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain1.As(&swapChain.dxgiSwapChain4));
	ThrowIfFailed(swapChain.dxgiSwapChain4->SetMaximumFrameLatency(SWAP_CHAIN_BUFFER_COUNT - 1));

	swapChain.frameLatencyWaitHandle = swapChain.dxgiSwapChain4->GetFrameLatencyWaitableObject();
	swapChain.currentBackBufferIndex = swapChain.dxgiSwapChain4->GetCurrentBackBufferIndex();
	swapChain.rtvVDescriptorHeap = CreateDescriptorHeap(Context::GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SWAP_CHAIN_BUFFER_COUNT);
	swapChain.rtvDescriptorSize = Context::GetDevice().dxgiDevice2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	swapChain.allowTearing = CheckTearingSupport();

	swapChain.UpdateBackBuffers();
}

void SwapChain::Present() {
	UINT syncInterval = vSync ? 1 : 0;
	UINT presentFlags = allowTearing && !fullscreen && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowIfFailed(dxgiSwapChain4->Present(syncInterval, presentFlags));
	
	CommandQueue& commandQueue = Context::CommandQueueDirect();
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
	const Device& device = Context::GetDevice();
	u32 rtvDescriptorSize = device.dxgiDevice2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvVDescriptorHeap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (u32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i) {
		Resource& backBuffer = backBuffers[i];
		ThrowIfFailed(dxgiSwapChain4->GetBuffer(i, IID_PPV_ARGS(&backBuffer.resource)));
		ThrowIfFailed(backBuffer.resource->SetName((L"Back Buffer[" + std::to_wstring(i) + L"]").c_str()));
		device.dxgiDevice2->CreateRenderTargetView(backBuffer.resource.Get(), nullptr, rtvHandle);
		rtvHandle.Offset((INT)rtvDescriptorSize);
	}
}

void Context::Create(HINSTANCE hInst)
{
	context = new Context;
	context->hInstance = hInst;

	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif

	// Register a window class for creating our render window with.
	WNDCLASSEXW windowClass = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = &WndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInst,
		.hIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)),
		.hCursor = ::LoadCursor(NULL, IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
		.lpszMenuName = NULL,
		.lpszClassName = WINDOW_CLASS_NAME,
		.hIconSm = ::LoadIcon(hInst, NULL),
	};

	if (!RegisterClassExW(&windowClass)) {
		MessageBoxA(NULL, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
	}

	context->adapter = Adapter::Create(false);
	context->device = Device::Create(context->adapter);

	context->commandQueueDirect.Init(context->device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	context->commandQueueCompute.Init(context->device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	context->commandQueueCopy.Init(context->device, D3D12_COMMAND_LIST_TYPE_COPY);
}

void Context::Destroy() {

	FlushAllCommandQueues();

	context->commandQueueDirect.ClearCommandLists();
	context->commandQueueCompute.ClearCommandLists();
	context->commandQueueCopy.ClearCommandLists();
	
	delete context;
	context = nullptr;
}

Window Context::CreateWindow(const wchar_t* title, ivec2 size, bool vSync) {
	Window window = {};

	window.rect = { 0, 0, (LONG)size.x, (LONG)size.y };
	AdjustWindowRect(&window.rect, WS_OVERLAPPEDWINDOW, FALSE);

	window.hWnd = CreateWindowExW(
		NULL, WINDOW_CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		window.rect.right - window.rect.left,
		window.rect.bottom - window.rect.top,
		NULL, NULL, context->hInstance, nullptr);

	if (!window.hWnd) {
		MessageBoxA(NULL, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
		return {};
	}

	CreateWindowSwapChain(window);
	CreateConsole(); // Create debug console while using WINDOWS subsystem

	window.initialized = true;

	return window;
}

void Context::DestroyWindow(Window& window) {
	
}

// Make sure the command queue has finished all commands before closing.
void Context::FlushAllCommandQueues() {
	context->commandQueueDirect.Flush();
	context->commandQueueCompute.Flush();
	context->commandQueueCopy.Flush();
}

void Context::Quit(int exitCode) {
	PostQuitMessage(exitCode);
}

Device& Context::GetDevice() { return context->device; }
CommandQueue& Context::CommandQueueDirect() { return context->commandQueueDirect; }
CommandQueue& Context::CommandQueueCompute() { return context->commandQueueCompute; }
CommandQueue& Context::CommandQueueCopy() { return context->commandQueueCopy; }

void SetWindowPointer(Window& window) {

	// Set pointer to this WinApp, to allow it to be retrieved in WndProc.
	SetWindowLongPtrW(window.hWnd, GWLP_USERDATA, (LONG_PTR)&window);
}

void ResizeWindow(Window& window, ivec2 size) {
	if (window.size.x != size.x || window.size.y != size.y)
	{
		// Don't allow 0 size swap chain back buffers.
		window.size.x = std::max(1, size.x);
		window.size.y = std::max(1, size.y);
		
		SwapChain& swapChain = window.swapChain;

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		Context::FlushAllCommandQueues();

		for (Resource& backBuffer : swapChain.backBuffers) {
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			backBuffer.resource.Reset();
		}
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(swapChain.dxgiSwapChain4->GetDesc(&swapChainDesc));
		ThrowIfFailed(swapChain.dxgiSwapChain4->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, window.size.x, window.size.y,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		swapChain.currentBackBufferIndex = swapChain.dxgiSwapChain4->GetCurrentBackBufferIndex();
		swapChain.UpdateBackBuffers();
	}
}

void SetShowWindow(Window& window, bool show) {
	ShowWindow(window.hWnd, show ? SW_SHOW : SW_HIDE);
}

bool WindowPollEvents(Window& window) {

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
	f64 delta = f64(currentTime.QuadPart - window.lastTime);
	f64 total = f64(currentTime.QuadPart - window.startTime);
	window.lastTime = currentTime.QuadPart;

	// Store time in seconds
	window.deltaTime = delta * window.invPerfFreq;
	window.totalTime = total * window.invPerfFreq;

	// Update the window title with current FPS every second
	if (window.totalTime - floor(window.totalTime) < window.deltaTime) {
		constexpr size_t BUFFER_SIZE = 256;
		WCHAR buffer[BUFFER_SIZE];
		f64 fps = round(1.0 / window.deltaTime);
		f64 mspf = 1000.0 / fps;
		swprintf_s(buffer, BUFFER_SIZE, L"FPS: %3.0f | MS/Frame: %2.1f", fps, mspf);
		SetWindowTextW(window.hWnd, buffer);
	}
	
	return true;
}

LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	
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
			ResizeWindow(*window, {
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
