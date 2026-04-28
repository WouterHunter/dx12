
// Win32 includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#ifdef CreateWindow
#undef CreateWindow
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

// Icon resource
#include "resource.h"


// DirectX headers
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

// STL
#include <filesystem>
#include <chrono>

#include "core.h"
#include "utils.h"


namespace fs = std::filesystem;

using std::chrono::nanoseconds;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;

#ifdef _DEBUG
constexpr u32 CREATE_FACTORY_FLAGS = DXGI_CREATE_FACTORY_DEBUG;
#elif
constexpr u32 CREATE_FACTORY_FLAGS = 0;
#endif


// Adapter
struct Adapter {
	ComPtr<IDXGIAdapter4> dxgiAdapter4;
};

Adapter GetAdapter(b8 useWarp) {
	ComPtr<IDXGIFactory4> factory;

	ThrowIfFailed(CreateDXGIFactory2(CREATE_FACTORY_FLAGS, IID_PPV_ARGS(&factory)));

	Adapter result = {};
	ComPtr<IDXGIAdapter1> adapter1;

	if (useWarp) {
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter1)));
		ThrowIfFailed(adapter1.As(&result.dxgiAdapter4));
	}
	else {
		// Find the graphics card with the most dedicated video memory.
		SIZE_T maxMemorySize = 0;
		for (UINT i = 0; factory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i) {
			DXGI_ADAPTER_DESC1 desc;
			adapter1->GetDesc1(&desc);

			if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(adapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				desc.DedicatedVideoMemory > maxMemorySize) {
				maxMemorySize = desc.DedicatedVideoMemory;
				ThrowIfFailed(adapter1.As(&result.dxgiAdapter4));
			}
		}
	}

	return result;
}

// Device
struct Device {
	ComPtr<ID3D12Device2> dxgiDevice2;
};

Device CreateDevice(const Adapter& adapter) {

	Device device = {};

	ThrowIfFailed(D3D12CreateDevice(adapter.dxgiAdapter4.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device.dxgiDevice2)));

	// Enable debug messages in debug mode.
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device.dxgiDevice2.As(&infoQueue))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER filter = {
			.DenyList = {
				.NumSeverities = _countof(severities),
				.pSeverityList = severities,
				.NumIDs = _countof(denyIds),
				.pIDList = denyIds
			}
		};

		ThrowIfFailed(infoQueue->PushStorageFilter(&filter));
	}
#endif

	return device;
}

// Command queue
struct CommandQueue {
	ComPtr<ID3D12CommandQueue> d3dCommandQueue;
};

CommandQueue CreateCommandQueue(Device& device, D3D12_COMMAND_LIST_TYPE type) {
	CommandQueue commandQueue = {};
	D3D12_COMMAND_QUEUE_DESC desc = {
		.Type = type,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	ThrowIfFailed(device.dxgiDevice2->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue.d3dCommandQueue)));
	return commandQueue;
}

b8 CheckTearingSupport() {
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

// Swap chain
struct SwapChain {
	ComPtr<IDXGISwapChain4> swapChain4;
};

SwapChain CreateSwapChain(HWND hWnd, CommandQueue commandQueue, ivec2 size, u32 bufferCount) {
	SwapChain result = {};
	ComPtr<IDXGISwapChain1> swapChain1;
	ComPtr<IDXGIFactory4> factory;

	ThrowIfFailed(CreateDXGIFactory2(CREATE_FACTORY_FLAGS, IID_PPV_ARGS(&factory)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
		.Width = (UINT)size.x,
		.Height = (UINT)size.y,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = FALSE,
		.SampleDesc = { 1, 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = bufferCount,
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u,
	};

	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.d3dCommandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	ThrowIfFailed(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(swapChain1.As(&result.swapChain4));

	return result;
}

// Descriptor heap 
struct DescriptorHeap {
	ComPtr<ID3D12DescriptorHeap> d3dDescriptorHeap;
};

DescriptorHeap CreateDescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 numDescriptors) {
	DescriptorHeap heap = {};
	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type = type,
		.NumDescriptors = numDescriptors,
	};
	ThrowIfFailed(device.dxgiDevice2->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.d3dDescriptorHeap)));

	return heap;
}

// Resource
struct Resource {
	ComPtr<ID3D12Resource> resource;
};

// Command allocator
struct CommandAllocator {
	ComPtr<ID3D12CommandAllocator> d3dCommandAllocator;
};

CommandAllocator CreateCommandAllocator(const Device& device, D3D12_COMMAND_LIST_TYPE type) {
	CommandAllocator allocator = {};
	ThrowIfFailed(device.dxgiDevice2->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator.d3dCommandAllocator)));
	return allocator;
}

// Command list
struct CommandList {
	ComPtr<ID3D12GraphicsCommandList> d3dCommandList;
};

CommandList CreateCommandList(const Device& device, const CommandAllocator& allocator, D3D12_COMMAND_LIST_TYPE type) {
	CommandList commandList = {};
	ThrowIfFailed(device.dxgiDevice2->CreateCommandList(
		0,
		type,
		allocator.d3dCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList.d3dCommandList)));
	ThrowIfFailed(commandList.d3dCommandList->Close());
	return commandList;
}

// Fence
struct Fence {
	ComPtr<ID3D12Fence> d3dFence;
};


Fence CreateFence(const Device& device)
{
	Fence fence = {};
	ThrowIfFailed(device.dxgiDevice2->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.d3dFence)));
	return fence;
}

HANDLE CreateEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

u64 Signal(const CommandQueue& commandQueue, const Fence& fence, u64& fenceValue) {
	u64 fenceValueForSignal = ++fenceValue;
	ThrowIfFailed(commandQueue.d3dCommandQueue->Signal(fence.d3dFence.Get(), fenceValueForSignal));
	return fenceValueForSignal;
}

void WaitForFenceValue(const Fence& fence, u64 fenceValue, HANDLE fenceEvent,
	milliseconds duration = milliseconds::max())
{
	if (fence.d3dFence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(fence.d3dFence->SetEventOnCompletion(fenceValue, fenceEvent));
		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

void Flush(const CommandQueue& commandQueue, const Fence& fence, u64& fenceValue, HANDLE fenceEvent)
{
	uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

namespace {
	constexpr u8 NUM_FRAMES = 3;

	struct WindowState {
		ivec2 size = { 1280, 720 };
		b8 vSync = true;
		b8 tearingSupported = false;
		b8 fullscreen = false;
		b8 useWarp = false;
		b8 isInitialized = false;

		HWND hWnd; // Window handle.
		RECT windowRect; // Window rectangle (used to toggle fullscreen state).

		// DirectX 12 Objects
		Device device;
		CommandQueue commandQueue;
		SwapChain swapChain;
		Resource backBuffers[NUM_FRAMES];
		CommandList commandList;
		CommandAllocator commandAllocators[NUM_FRAMES];
		DescriptorHeap rtvVDescriptorHeap;
		UINT rtvDescriptorSize;
		UINT currentBackBufferIndex;

		// Synchronization Objects
		Fence fence;
		u64 fenceValue = 0;
		u64 frameFenceValues[NUM_FRAMES] = {};
		HANDLE fenceEvent;
	};

	WindowState window;
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void ParseCommandLineArguments()
{
	int argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; ++i) {
		if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
			window.size.x = ::wcstol(argv[++i], nullptr, 10);
		if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
			window.size.y = ::wcstol(argv[++i], nullptr, 10);
		if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
			window.useWarp = true;
	}

	// Free memory allocated by CommandLineToArgvW
	LocalFree(argv);
}

void EnableDebugLayer() {
#if defined _DEBUG
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
{
	// Register a window class for creating our render window with.
	WNDCLASSEXW windowClass = {};

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInst;
	windowClass.hIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));	
	windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = windowClassName;
	windowClass.hIconSm = ::LoadIcon(hInst, NULL);

	static ATOM atom = RegisterClassExW(&windowClass);
	assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst,
	const wchar_t* windowTitle, ivec2 size)
{
	i32 screenWidth = GetSystemMetrics(SM_CXSCREEN);
	i32 screenHeight = GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect = { 0, 0, (LONG)size.x, (LONG)size.y };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	i32 windowWidth = windowRect.right - windowRect.left;
	i32 windowHeight = windowRect.bottom - windowRect.top;

	// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
	i32 windowX = std::max(0, (screenWidth - windowWidth) / 2);
	i32 windowY = std::max(0, (screenHeight - windowHeight) / 2);
	
	HWND hWnd = CreateWindowExW(
		NULL,
		windowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		hInst,
		nullptr
	);

	assert(hWnd && "Failed to create window");

	return hWnd;
}

void UpdateRenderTargetViews(const Device& device, const SwapChain& swapChain, const DescriptorHeap& heap) {
	u32 rtvDescriptorSize = device.dxgiDevice2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(heap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (u32 i = 0; i < NUM_FRAMES; ++i) {
		Resource& backBuffer = window.backBuffers[i];
		ThrowIfFailed(swapChain.swapChain4->GetBuffer(i, IID_PPV_ARGS(&backBuffer.resource)));
		device.dxgiDevice2->CreateRenderTargetView(backBuffer.resource.Get(), nullptr, rtvHandle);
		rtvHandle.Offset(rtvDescriptorSize);
	}
}

void Update()
{
	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	auto t1 = clock.now();
	auto deltaTime = t1 - t0;
	t0 = t1;

	elapsedSeconds += deltaTime.count() * 1e-9;
	if (elapsedSeconds > 1.0)
	{
		wchar_t buffer[500];
		auto fps = frameCounter / elapsedSeconds;
		swprintf_s(buffer, 500, L"FPS: %f\n", fps);
		OutputDebugString(buffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}

void Render()
{
	CommandAllocator& commandAllocator = window.commandAllocators[window.currentBackBufferIndex];
	CommandList& commandList = window.commandList;
	Resource& backBuffer = window.backBuffers[window.currentBackBufferIndex];

	commandAllocator.d3dCommandAllocator->Reset();
	commandList.d3dCommandList->Reset(commandAllocator.d3dCommandAllocator.Get(), nullptr);

	// Clear the render target.
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.resource.Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		commandList.d3dCommandList->ResourceBarrier(1, &barrier);

		FLOAT clearColor[] = { 0.1f, 0.15f, 0.15f, 1.0f };
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(window.rtvVDescriptorHeap.d3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			window.currentBackBufferIndex, window.rtvDescriptorSize);

		commandList.d3dCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	}    
	
	// Present
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.resource.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList.d3dCommandList->ResourceBarrier(1, &barrier);
		ThrowIfFailed(commandList.d3dCommandList->Close());

		ID3D12CommandList* const commandLists[] = {
			commandList.d3dCommandList.Get()
		};
		window.commandQueue.d3dCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
		
		UINT syncInterval = window.vSync? 1 : 0;
		UINT presentFlags = window.tearingSupported && !window.vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(window.swapChain.swapChain4->Present(syncInterval, presentFlags));

		window.frameFenceValues[window.currentBackBufferIndex] = Signal(
			window.commandQueue, window.fence, window.fenceValue);

		window.currentBackBufferIndex = window.swapChain.swapChain4->GetCurrentBackBufferIndex();
		WaitForFenceValue(window.fence, window.frameFenceValues[window.currentBackBufferIndex], window.fenceEvent);
	}
}

void Resize(ivec2 size)
{
	if (window.size.x != size.x || window.size.y != size.y)
	{
		// Don't allow 0 size swap chain back buffers.
		window.size.x = std::max(1, size.x);
		window.size.y = std::max(1, size.y);

		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		Flush(window.commandQueue, window.fence, window.fenceValue, window.fenceEvent);
		
		for (int i = 0; i < NUM_FRAMES; ++i)
		{
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			window.backBuffers[i].resource.Reset();
			window.frameFenceValues[i] = window.frameFenceValues[window.currentBackBufferIndex];
		}
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(window.swapChain.swapChain4->GetDesc(&swapChainDesc));
		ThrowIfFailed(window.swapChain.swapChain4->ResizeBuffers(NUM_FRAMES, window.size.x, window.size.y,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		window.currentBackBufferIndex= window.swapChain.swapChain4->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews(window.device, window.swapChain, window.rtvVDescriptorHeap);
	}
}


LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (window.isInitialized) {
		switch (message) {
		case WM_PAINT:
			Update();
			Render();
			break;

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

			switch (wParam)
			{
			case 'V':
				window.vSync = !window.vSync;
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
			Resize({
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

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Window class name. Used for registering / creating the window.
	const wchar_t* windowClassName = L"DX12WindowClass";
	ParseCommandLineArguments();

	EnableDebugLayer();

	window.tearingSupported = CheckTearingSupport();
	RegisterWindowClass(hInstance, windowClassName);
	window.hWnd = CreateWindow(windowClassName, hInstance, L"dx12 lib", window.size);
	GetWindowRect(window.hWnd, &window.windowRect);

	Adapter adapter = GetAdapter(window.useWarp);
	window.device = CreateDevice(adapter);
	window.commandQueue = CreateCommandQueue(window.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	window.swapChain = CreateSwapChain(window.hWnd, window.commandQueue, window.size, NUM_FRAMES);
	window.currentBackBufferIndex = window.swapChain.swapChain4->GetCurrentBackBufferIndex();
	window.rtvVDescriptorHeap = CreateDescriptorHeap(window.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES);
	window.rtvDescriptorSize = window.device.dxgiDevice2->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(window.device, window.swapChain, window.rtvVDescriptorHeap);

	for (u32 frameIdx = 0; frameIdx < NUM_FRAMES; ++frameIdx) {
		CommandAllocator& allocator = window.commandAllocators[frameIdx];
		ThrowIfFailed(window.device.dxgiDevice2->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, 
			IID_PPV_ARGS(&allocator.d3dCommandAllocator)));
		//window.commandAllocators[frameIdx] = CreateCommandAllocator(
		//	window.device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	window.commandList = CreateCommandList(window.device, 
		window.commandAllocators[window.currentBackBufferIndex], 
		D3D12_COMMAND_LIST_TYPE_DIRECT);

	window.fence = CreateFence(window.device);
	window.fenceEvent = CreateEventHandle();

	window.isInitialized = true;

	::ShowWindow(window.hWnd, SW_SHOW);

	// Pump messages
	MSG msg = {};
	while(msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	// Cleanup 
	// Make sure the command queue has finished all commands before closing.
	Flush(window.commandQueue, window.fence, window.fenceValue, window.fenceEvent);
	::CloseHandle(window.fenceEvent);

	return 0;
}
