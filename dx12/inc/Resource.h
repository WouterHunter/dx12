#pragma once

/** Resource */
struct Resource {

    D3D12_RESOURCE_DESC GetD3D12ResourceDesc() const;
	D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats() const;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	ComPtr<ID3D12Resource> d3d12Resource;
};

struct RenderTargetView {

};

struct DepthStencilView {

};
