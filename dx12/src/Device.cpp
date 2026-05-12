#include "DX12PCH.h"
#include "Device.h"

Adapter Adapter::Create(bool useWarp) {
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

Device Device::Create(const Adapter& adapter) {
	Device device = {};

	ThrowIfFailed(D3D12CreateDevice(adapter.dxgiAdapter4.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device.dxgiDevice2)));

	// Enable debug messages in debug mode.
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device.dxgiDevice2.As(&infoQueue))) {
		ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
		ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
		ThrowIfFailed(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));

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
