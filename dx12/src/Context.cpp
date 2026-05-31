#include "DX12PCH.h"
#include "Context.h"

#include <comdef.h> // For _com_error class (used to decode HR result codes).
#include <fcntl.h> 
#include <corecrt_io.h>

constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12 Render Window";

namespace {

	std::string GetWin32ErrorMessage() {
		DWORD errorCode = GetLastError();

		if (errorCode == 0)
			return "";

		LPWSTR wideBuffer = nullptr;

		DWORD size = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			errorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&wideBuffer,
			0,
			nullptr);

		if (size == 0 || !wideBuffer) 
			return "Unknown Win32 error";

		int utf8Size = WideCharToMultiByte(
			CP_UTF8,
			0,
			wideBuffer,
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);

		std::string result;
		result.resize(utf8Size - 1);

		WideCharToMultiByte(
			CP_UTF8,
			0,
			wideBuffer,
			-1,
			result.data(),
			utf8Size,
			nullptr,
			nullptr);

		LocalFree(wideBuffer);

		return result;
	}

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

	void CreateWindowSwapChain(Window* window, bool vSync, bool fullscreen) {
		SwapChain& swapChain = window->swapChain;
		ComPtr<IDXGISwapChain1> swapChain1;
		ComPtr<IDXGIFactory4> factory;

		swapChain.allowTearing = CheckTearingSupport();
		swapChain.vSync = vSync;
		swapChain.fullscreen = fullscreen;

		ThrowIfFailed(CreateDXGIFactory2(CREATE_FACTORY_FLAGS, IID_PPV_ARGS(&factory)));

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
			.Width = (UINT)window->size.x,
			.Height = (UINT)window->size.y,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.Stereo = FALSE,
			.SampleDesc = { 1, 0 },
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = SWAP_CHAIN_BUFFER_COUNT,
			.Scaling = DXGI_SCALING_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
			.Flags = (swapChain.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
				DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
		};

		Context* context = window->context;
		swapChain.context = context;
		const CommandQueue& commandQueue = context->CommandQueueDirect();

		ThrowIfFailed(factory->CreateSwapChainForHwnd(
			commandQueue.d3dCommandQueue.Get(),
			window->hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain1));

		ThrowIfFailed(factory->MakeWindowAssociation(window->hWnd, DXGI_MWA_NO_ALT_ENTER));
		ThrowIfFailed(swapChain1.As(&swapChain.dxgiSwapChain4));
		ThrowIfFailed(swapChain.dxgiSwapChain4->SetMaximumFrameLatency(SWAP_CHAIN_BUFFER_COUNT - 1));

		swapChain.frameLatencyWaitHandle = swapChain.dxgiSwapChain4->GetFrameLatencyWaitableObject();
		swapChain.currentBackBufferIndex = swapChain.dxgiSwapChain4->GetCurrentBackBufferIndex();
		swapChain.rtvVDescriptorHeap = CreateDescriptorHeap(context->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SWAP_CHAIN_BUFFER_COUNT);
		swapChain.rtvDescriptorSize = context->GetDevice().d3d12Device10->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		swapChain.UpdateBackBuffers();
	}
}

Context* Context::Create(HINSTANCE hInst, int icon) {
	Context* context = new Context;
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

	// Report live objects after exiting the application.
	std::atexit(ReportLiveObjects);
#endif

	// Initialize the COM library.
	if (const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); FAILED(hr)) {
		const _com_error err(hr);
		BREAK("CoInitialize failed: {}", err.ErrorMessage());
		return nullptr;
	}

	// Register a window class for creating our render window with.
	WNDCLASSEXW windowClass = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = &Window::WndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInst,
		.hIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(icon)),
		.hCursor = ::LoadCursor(NULL, IDC_ARROW),
		.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
		.lpszMenuName = NULL,
		.lpszClassName = WINDOW_CLASS_NAME,
		.hIconSm = ::LoadIcon(hInst, NULL),
	};

	if (!RegisterClassExW(&windowClass)) {
		BREAK("Unable to register the window class: {}", GetWin32ErrorMessage());
		return nullptr;
	}

	// Create core objects
	context->adapter = Adapter::Create(false);
	context->device = Device::Create(context->adapter);

	context->commandQueueDirect.Init(context, D3D12_COMMAND_LIST_TYPE_DIRECT);
	context->commandQueueCompute.Init(context, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	context->commandQueueCopy.Init(context, D3D12_COMMAND_LIST_TYPE_COPY);
	context->globalLayoutTracker.Init(context);

	// Init Descriptor Allocators
	for (u32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
		context->descriptorAllocators[i].Init(context,
			(D3D12_DESCRIPTOR_HEAP_TYPE)i, 256);
	}

	return context;
}

void Context::Destroy(Context* context) {

	context->FlushAllCommandQueues();

	context->commandQueueDirect.ClearCommandLists();
	context->commandQueueCompute.ClearCommandLists();
	context->commandQueueCopy.ClearCommandLists();

	delete context;
}

Window* Context::CreateWindow(const wchar_t* title, ivec2 size, bool vSync) {
	Window* window = new Window{};
	window->context = this;
	window->rect = { 0, 0, (LONG)size.x, (LONG)size.y };
	AdjustWindowRect(&window->rect, WS_OVERLAPPEDWINDOW, FALSE);

	window->hWnd = CreateWindowExW(
		NULL, WINDOW_CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		window->rect.right - window->rect.left,
		window->rect.bottom - window->rect.top,
		NULL, NULL, hInstance, nullptr);

	if (!window->hWnd) {
		MessageBoxA(NULL, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
		return {};
	}

	// Set pointer to this WinApp, to allow it to be retrieved in WndProc.
	SetWindowLongPtrW(window->hWnd, GWLP_USERDATA, (LONG_PTR)window);

	CreateConsole(); // Create debug console while using WINDOWS subsystem
	CreateWindowSwapChain(window, vSync, false);

	window->initialized = true;

	return window;
}

void Context::DestroyWindow(Window* window) {
	delete window;
}

// Make sure the command queue has finished all commands before closing.
void Context::FlushAllCommandQueues() {
	commandQueueDirect.Flush();
	commandQueueCompute.Flush();
	commandQueueCopy.Flush();
}

void Context::Quit(int exitCode) {
	PostQuitMessage(exitCode);
}

Ref<Resource> Context::CreateResource(D3D12_RESOURCE_DESC1 desc, D3D12_HEAP_TYPE heapType, const wchar_t* name) {
	
	ComPtr<ID3D12Resource> d3d12Resource;
	CD3DX12_HEAP_PROPERTIES defaultProperties(heapType);
	ThrowIfFailed(device.d3d12Device10->CreateCommittedResource3(
		&defaultProperties, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr,
		IID_PPV_ARGS(&d3d12Resource)));

	Ref<Resource> resource = MakeRef<Resource>(this, d3d12Resource, name);

	// Check feature support
	resource->formatSupport.Format = desc.Format;
	ThrowIfFailed(device.d3d12Device10->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
		&resource->formatSupport, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));


	return resource;
}

Ref<Texture> Context::CreateTexture(D3D12_RESOURCE_DESC1 desc1, D3D12_HEAP_TYPE heapType, D3D12_BARRIER_LAYOUT layout, const wchar_t* name) {
	ComPtr<ID3D12Resource> d3d12Resource;
	CD3DX12_HEAP_PROPERTIES defaultProperties(heapType);
	ThrowIfFailed(device.d3d12Device10->CreateCommittedResource3(
		&defaultProperties, D3D12_HEAP_FLAG_NONE,
		&desc1, layout, nullptr, nullptr, 0, nullptr,
		IID_PPV_ARGS(&d3d12Resource)));

	// Find the subresource count and track the texture layout
	D3D12_RESOURCE_DESC desc = d3d12Resource->GetDesc();
	u32 arraySize = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : desc.DepthOrArraySize;
	u32 subresourceCount = desc.MipLevels * arraySize;
	globalLayoutTracker.Register(d3d12Resource.Get(), layout, subresourceCount);

	Ref<Texture> texture = MakeRef<Texture>(this, d3d12Resource, nullptr, name);

	return texture;
}

DescriptorAllocation Context::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
	return descriptorAllocators[type].Allocate(numDescriptors);
}
