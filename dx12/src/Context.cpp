#include "DX12PCH.h"
#include "Context.h"

#include <comdef.h> // For _com_error class (used to decode HR result codes).
#include <fcntl.h> 
#include <corecrt_io.h>

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

			// Enable virtual terminal processing to use ANSI escape codes
			DWORD mode = 0;
			if (GetConsoleMode(lStdHandle, &mode)) {
				mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				SetConsoleMode(lStdHandle, mode);
			}

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
}

Context* Context::Create(HINSTANCE hInst, int icon) {
	Context* context = new Context;
	context->hInstance = hInst;

	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Create debug console (only necessary while using WINDOWS subsystem).
	CreateConsole();

#ifdef _DEBUG
	// Enabled D3D12 debug layer.
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

	// Create mipmapping PSO
	context->mipmappingPSO = MakeRef<MipmappingPSO>(context);

	return context;
}

void Context::Destroy(Context* context) {

	context->FlushAllCommandQueues();

	context->commandQueueDirect.ClearCommandLists();
	context->commandQueueCompute.ClearCommandLists();
	context->commandQueueCopy.ClearCommandLists();

	delete context;
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

Ref<Resource> Context::CreateResource(
	const D3D12_RESOURCE_DESC1& desc1,
	D3D12_HEAP_TYPE heapType, 
	const wchar_t* name
) {
	ComPtr<ID3D12Resource> d3d12Resource;
	CD3DX12_HEAP_PROPERTIES heapProperties(heapType);

	ThrowIfFailed(device.d3d12Device10->CreateCommittedResource3(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &desc1,
		D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr,
		IID_PPV_ARGS(&d3d12Resource)));

	Ref<Resource> resource = MakeRef<Resource>(this, d3d12Resource, name);
	return resource;
}

Ref<Texture> Context::CreateTexture(
	const D3D12_RESOURCE_DESC1& desc1, 
	D3D12_BARRIER_LAYOUT layout, 
	D3D12_HEAP_TYPE heapType, 
	const D3D12_CLEAR_VALUE* clearValue, 
	const wchar_t* name
) {
	ComPtr<ID3D12Resource> d3d12Resource;
	CD3DX12_HEAP_PROPERTIES heapProperties(heapType);

	ThrowIfFailed(device.d3d12Device10->CreateCommittedResource3(
		&heapProperties, D3D12_HEAP_FLAG_NONE,
		&desc1, layout, clearValue, nullptr, 0, nullptr,
		IID_PPV_ARGS(&d3d12Resource)));

	globalLayoutTracker.Register(d3d12Resource.Get(), layout);

	Ref<Texture> texture = MakeRef<Texture>(this, d3d12Resource, clearValue, name);
	return texture;
}

DescriptorAllocation Context::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
	return descriptorAllocators[type].Allocate(numDescriptors);
}
