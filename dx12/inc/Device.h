#pragma once



/** Adapter */
class Adapter {
public:
	static Adapter Create(bool useWarp);

	ComPtr<IDXGIAdapter4> dxgiAdapter4;
};

/** Device */
class Device {
public:
	static Device Create(const Adapter& adapter);

	DXGI_SAMPLE_DESC GetMultiSampleDesc(DXGI_FORMAT format, u32 numDesiredSamples = 32,
		D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE);

	ComPtr<ID3D12Device10> d3d12Device10;
	D3D_ROOT_SIGNATURE_VERSION highestRootSigVersion;
};
