#pragma once



/** Adapter */
struct Adapter {
	static Adapter Create(bool useWarp);

	ComPtr<IDXGIAdapter4> dxgiAdapter4;
};

/** Device */
struct Device {
	static Device Create(const Adapter& adapter);

	ComPtr<ID3D12Device2> d3d12Device2;
	D3D_ROOT_SIGNATURE_VERSION highestRootSigVersion;
};
