#include "DX12PCH.h"
#include "Resource.h"

D3D12_RESOURCE_DESC Resource::GetD3D12ResourceDesc() const {
	D3D12_RESOURCE_DESC desc = {};
	if (d3d12Resource) {
		desc = d3d12Resource->GetDesc();
	}
	return desc;
}

D3D12_RT_FORMAT_ARRAY Resource::GetRenderTargetFormats() const {
	D3D12_RESOURCE_DESC desc = GetD3D12ResourceDesc();
	D3D12_RT_FORMAT_ARRAY formats = {
		.RTFormats = { desc.Format },
		.NumRenderTargets = 1,
	};
	return formats;
}
